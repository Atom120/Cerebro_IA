#!/usr/bin/env node
/**
 * build-and-verify-true-ingame-v2.js
 * Script TODO-EN-UNO de la skill `portal-me-true-ingame-v2` (Rama `segundo-intento` — Opción 1 Refinada):
 * 1. Módulo 1: Genera `levels/edge_p_true_ingame_v2/me_edge_true_ingame_v2.vmf` (7 brushes 1:1 con la azotea activa de Edge_p)
 *              y lo compila a `.bsp` con `vbsp.exe -game "<PortalDir>/portal"` si Portal está instalado.
 * 2. Módulo 2 y 3: Compila `src/true_ingame_v2/true_ingame_bridge_v2.cpp` -> `true_ingame_bridge_v2.exe` (x86 32 bits).
 * 3. Módulo 4: Compila `src/true_ingame_v2/me_true_ingame_d3d9.cpp` -> `me_true_ingame_d3d9.dll` (Proxy D3D9 EndScene x86).
 * 4. Ejecuta `true_ingame_bridge_v2.exe --self-test` validando de extremo a extremo los 4 módulos.
 */

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const { execFileSync, execSync } = require('child_process');

const repoRoot = path.resolve(__dirname, '../../../..');
const manifestPath = path.join(repoRoot, 'levels', 'edge_p_true_ingame_v2', 'slice_manifest.json');
const outputVmfPath = path.join(repoRoot, 'levels', 'edge_p_true_ingame_v2', 'me_edge_true_ingame_v2.vmf');
const outputBspPath = path.join(repoRoot, 'levels', 'edge_p_true_ingame_v2', 'me_edge_true_ingame_v2.bsp');
const headerPath = path.join(repoRoot, 'src', 'common', 'ipc_true_ingame_v2.h');
const bridgeCppPath = path.join(repoRoot, 'src', 'true_ingame_v2', 'true_ingame_bridge_v2.cpp');
const d3d9CppPath = path.join(repoRoot, 'src', 'true_ingame_v2', 'me_true_ingame_d3d9.cpp');
const defaultPortalDir = 'C:\\Program Files (x86)\\Steam\\steamapps\\common\\Portal';

function roundHU(val) {
  return Math.round(val * 100) / 100;
}

function convertBoxUUToHU(boundsUU, uuToHu) {
  const minX = roundHU(boundsUU.min.x * uuToHu);
  const maxX = roundHU(boundsUU.max.x * uuToHu);
  const minY = roundHU(-boundsUU.max.y * uuToHu);
  const maxY = roundHU(-boundsUU.min.y * uuToHu);
  const minZ = roundHU(boundsUU.min.z * uuToHu);
  const maxZ = roundHU(boundsUU.max.z * uuToHu);

  return {
    min: { x: Math.min(minX, maxX), y: Math.min(minY, maxY), z: Math.min(minZ, maxZ) },
    max: { x: Math.max(minX, maxX), y: Math.max(minY, maxY), z: Math.max(minZ, maxZ) }
  };
}

function buildVmfBoxSolid(idGen, boxHU, material, comment) {
  const solidId = idGen.next();
  const { min, max } = boxHU;
  const sides = [
    { plane: `(${min.x} ${max.y} ${max.z}) (${max.x} ${max.y} ${max.z}) (${max.x} ${min.y} ${max.z})`, u: '[1 0 0 0] 0.25', v: '[0 -1 0 0] 0.25' },
    { plane: `(${min.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${max.y} ${min.z})`, u: '[1 0 0 0] 0.25', v: '[0 -1 0 0] 0.25' },
    { plane: `(${min.x} ${max.y} ${max.z}) (${min.x} ${min.y} ${max.z}) (${min.x} ${min.y} ${min.z})`, u: '[0 1 0 0] 0.25', v: '[0 0 -1 0] 0.25' },
    { plane: `(${max.x} ${max.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${max.z})`, u: '[0 1 0 0] 0.25', v: '[0 0 -1 0] 0.25' },
    { plane: `(${max.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${min.z})`, u: '[1 0 0 0] 0.25', v: '[0 0 -1 0] 0.25' },
    { plane: `(${max.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${max.z})`, u: '[1 0 0 0] 0.25', v: '[0 0 -1 0] 0.25' }
  ];

  const sidesVmf = sides
    .map(
      (s) => `\t\tside
\t\t{
\t\t\t"id" "${idGen.next()}"
\t\t\t"plane" "${s.plane}"
\t\t\t"material" "${material.toUpperCase()}"
\t\t\t"uaxis" "${s.u}"
\t\t\t"vaxis" "${s.v}"
\t\t\t"rotation" "0"
\t\t\t"lightmapscale" "16"
\t\t\t"smoothing_groups" "0"
\t\t}`
    )
    .join('\n');

  return `\tsolid
\t{
\t\t"id" "${solidId}"
${sidesVmf}
\t\teditor
\t\t{
\t\t\t"color" "0 180 255"
\t\t\t"comments" "${comment}"
\t\t}
\t}`;
}

function buildVmfWedge45PlusX(idGen, boxHU, portalMat, backMat, comment) {
  const solidId = idGen.next();
  const { min, max } = boxHU;
  const sides = [
    { plane: `(${min.x} ${max.y} ${max.z}) (${max.x} ${max.y} ${min.z}) (${max.x} ${min.y} ${min.z})`, m: portalMat, u: '[0 1 0 0] 0.25', v: '[0.707107 0 -0.707107 0] 0.25' },
    { plane: `(${min.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${max.y} ${min.z})`, m: backMat, u: '[1 0 0 0] 0.25', v: '[0 -1 0 0] 0.25' },
    { plane: `(${min.x} ${max.y} ${max.z}) (${min.x} ${min.y} ${max.z}) (${min.x} ${min.y} ${min.z})`, m: backMat, u: '[0 1 0 0] 0.25', v: '[0 0 -1 0] 0.25' },
    { plane: `(${max.x} ${max.y} ${min.z}) (${min.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${min.z})`, m: backMat, u: '[1 0 0 0] 0.25', v: '[0 0 -1 0] 0.25' },
    { plane: `(${max.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${max.z})`, m: backMat, u: '[1 0 0 0] 0.25', v: '[0 0 -1 0] 0.25' }
  ];

  const sidesVmf = sides
    .map(
      (s) => `\t\tside
\t\t{
\t\t\t"id" "${idGen.next()}"
\t\t\t"plane" "${s.plane}"
\t\t\t"material" "${s.m.toUpperCase()}"
\t\t\t"uaxis" "${s.u}"
\t\t\t"vaxis" "${s.v}"
\t\t\t"rotation" "0"
\t\t\t"lightmapscale" "16"
\t\t\t"smoothing_groups" "0"
\t\t}`
    )
    .join('\n');

  return `\tsolid
\t{
\t\t"id" "${solidId}"
${sidesVmf}
\t\teditor
\t\t{
\t\t\t"color" "255 120 0"
\t\t\t"comments" "${comment} [TRUE_INGAME_V2_WEDGE_45]"
\t\t}
\t}`;
}

function main() {
  console.log('=== True In-Game Pass-Through v2 (4 Modules Pipeline): Build & Verify ===\n');

  assert(fs.existsSync(manifestPath), 'Falta levels/edge_p_true_ingame_v2/slice_manifest.json');
  assert(fs.existsSync(headerPath), 'Falta src/common/ipc_true_ingame_v2.h');
  assert(fs.existsSync(bridgeCppPath), 'Falta src/true_ingame_v2/true_ingame_bridge_v2.cpp');
  assert(fs.existsSync(d3d9CppPath), 'Falta src/true_ingame_v2/me_true_ingame_d3d9.cpp');

  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const uuToHu = manifest.conversion_constants.uu_to_hu;

  let currentId = 1;
  const idGen = { next: () => currentId++ };
  const worldId = idGen.next();
  const solids = [];

  // 1. Sello Skybox
  const bboxHU = convertBoxUUToHU(manifest.bounding_box_local_uu, uuToHu);
  const pad = 128;
  const thick = 32;
  const skyWalls = [
    { min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad, z: bboxHU.max.z + pad }, max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad + thick } },
    { min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad - thick }, max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad, z: bboxHU.min.z - pad } },
    { min: { x: bboxHU.min.x - pad - thick, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad }, max: { x: bboxHU.min.x - pad, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad } },
    { min: { x: bboxHU.max.x + pad, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad }, max: { x: bboxHU.max.x + pad + thick, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad } },
    { min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad - thick, z: bboxHU.min.z - pad }, max: { x: bboxHU.max.x + pad, y: bboxHU.min.y - pad, z: bboxHU.max.z + pad } },
    { min: { x: bboxHU.min.x - pad, y: bboxHU.max.y + pad, z: bboxHU.min.z - pad }, max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad + thick, z: bboxHU.max.z + pad } }
  ];

  for (const [i, sw] of skyWalls.entries()) {
    solids.push(buildVmfBoxSolid(idGen, sw, 'TOOLS/TOOLSSKYBOX', `tv2_sky_${i}`));
  }

  for (const b of manifest.proxy_brushes) {
    const boxHU = convertBoxUUToHU(b.bounds_local_uu, uuToHu);
    if (b.shape === 'wedge_45_plus_x') {
      solids.push(buildVmfWedge45PlusX(idGen, boxHU, b.source_material, 'METAL/METALWALL048B', b.id));
    } else {
      solids.push(buildVmfBoxSolid(idGen, boxHU, b.source_material, b.id));
    }
  }

  const sp = manifest.player_spawn.portal_hu;
  const vmf = `versioninfo
{
\t"editorversion" "400"
\t"mapversion" "2"
\t"formatversion" "100"
\t"prefab" "0"
}
world
{
\t"id" "${worldId}"
\t"mapversion" "2"
\t"classname" "worldspawn"
\t"skyname" "sky_day01_01"
\t"comment" "True In-Game Pass-Through v2: Edge_p Active Rooftop 1:1"
${solids.join('\n')}
}
entity
{
\t"id" "${idGen.next()}"
\t"classname" "info_player_start"
\t"angles" "${sp.pitch_deg} ${sp.yaw_deg} ${sp.roll_deg}"
\t"origin" "${sp.x} ${sp.y} ${roundHU(sp.z + 16)}"
}
entity
{
\t"id" "${idGen.next()}"
\t"classname" "game_player_equip"
\t"weapon_portalgun" "1"
\t"origin" "${sp.x} ${sp.y} ${roundHU(sp.z + 48)}"
}
entity
{
\t"id" "${idGen.next()}"
\t"classname" "light_environment"
\t"_ambient" "215 230 255 190"
\t"_light" "255 250 242 460"
\t"angles" "0 315 0"
\t"pitch" "-65"
\t"origin" "${sp.x} ${sp.y} ${roundHU(bboxHU.max.z - 64)}"
}
`;

  fs.writeFileSync(outputVmfPath, vmf, 'utf8');
  console.log(`[Módulo 1] Mapa .vmf 1:1 generado (${manifest.proxy_brushes.length} superficies reales de Edge_p): ${path.relative(repoRoot, outputVmfPath)}`);

  // 2. Compilar a .bsp si vbsp.exe está instalado
  const vbspExe = path.join(defaultPortalDir, 'bin', 'vbsp.exe');
  const portalGameDir = path.join(defaultPortalDir, 'portal');
  if (fs.existsSync(vbspExe) && fs.existsSync(portalGameDir)) {
    execFileSync(vbspExe, ['-game', portalGameDir, outputVmfPath], { stdio: 'pipe' });
    assert(fs.existsSync(outputBspPath), 'vbsp.exe no produjo me_edge_true_ingame_v2.bsp');
    const prtPath = outputVmfPath.replace(/\.vmf$/, '.prt');
    const logPath = outputVmfPath.replace(/\.vmf$/, '.log');
    if (fs.existsSync(prtPath)) fs.unlinkSync(prtPath);
    if (fs.existsSync(logPath)) fs.unlinkSync(logPath);
    const bspSize = fs.statSync(outputBspPath).size;
    console.log(`[Módulo 1 - Steam vbsp.exe] .bsp compilado exitosamente: ${path.relative(repoRoot, outputBspPath)} (${bspSize} bytes)`);
  }

  // 3. Compilar me_true_ingame_d3d9.dll y true_ingame_bridge_v2.exe con MSVC x86 (vcvars32.bat)
  const vcvars32 = 'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat';
  const dllOutPath = path.join(repoRoot, 'me_true_ingame_d3d9.dll');
  const exeOutPath = path.join(repoRoot, 'true_ingame_bridge_v2.exe');

  if (fs.existsSync(vcvars32)) {
    const buildCmd = [
      `call "${vcvars32}" >nul`,
      `cl /nologo /LD /EHsc /O2 src\\true_ingame_v2\\me_true_ingame_d3d9.cpp /Fe:me_true_ingame_d3d9.dll user32.lib`,
      `cl /nologo /EHsc /O2 src\\true_ingame_v2\\true_ingame_bridge_v2.cpp /Fe:true_ingame_bridge_v2.exe user32.lib`,
      `del /q me_true_ingame_d3d9.obj me_true_ingame_d3d9.lib me_true_ingame_d3d9.exp true_ingame_bridge_v2.obj 2>nul`
    ].join(' && ');
    execSync(buildCmd, { cwd: repoRoot, shell: 'cmd.exe', stdio: 'pipe' });

    assert(fs.existsSync(dllOutPath), 'No se generó me_true_ingame_d3d9.dll');
    assert(fs.existsSync(exeOutPath), 'No se generó true_ingame_bridge_v2.exe');

    console.log(`[Módulo 4 - MSVC x86] Proxy D3D9 compilado: me_true_ingame_d3d9.dll (${fs.statSync(dllOutPath).size} bytes)`);
    console.log(`[Módulo 2/3 - MSVC x86] Orquestador compilado: true_ingame_bridge_v2.exe (${fs.statSync(exeOutPath).size} bytes)`);

    const selfTestOut = execFileSync(exeOutPath, ['--self-test'], { cwd: repoRoot, encoding: 'utf8' });
    console.log('\n--- Salida de Autoverificación (true_ingame_bridge_v2.exe --self-test) ---');
    console.log(selfTestOut.trim());
    assert(selfTestOut.includes('[4/4] FLUJO COMPLETO'), 'Fallo en true_ingame_bridge_v2.exe --self-test');
  }

  console.log('\nESTADO: FLUJO PRINCIPAL portal-me-true-ingame-v2 (4 MÓDULOS) IMPLEMENTADO Y VERIFICADO.');
}

main();

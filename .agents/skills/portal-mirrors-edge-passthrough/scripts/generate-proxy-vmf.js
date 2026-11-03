#!/usr/bin/env node
/**
 * generate-proxy-vmf.js
 * Genera automáticamente el archivo de mapa de Source Engine (Hammer .vmf)
 * para la sección única "edge_p_rooftop_01" a partir de levels/edge_p_rooftop_01/slice_manifest.json.
 *
 * Aplica la conversión geométrica exacta:
 * - Mirror's Edge (UE3, Left-Handed, 1 UU = 1 cm) -> Portal (Source, Right-Handed, 1 HU = 1.905 cm)
 * - Inversión de eje Y (LH -> RH) para mantener el orden de vértices y la orientación espacial.
 * - Clasificación de materiales en superficies "portalable" (concreto blanco) vs "noportal" (metal/vallas).
 */

const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '../../../..');
const manifestPath = path.join(repoRoot, 'levels', 'edge_p_rooftop_01', 'slice_manifest.json');
const outputVmfPath = path.join(repoRoot, 'levels', 'edge_p_rooftop_01', 'me_edge_rooftop_01.vmf');

function roundHU(val) {
  return Math.round(val * 100) / 100;
}

/**
 * Convierte una caja delimitadora en coordenadas locales de Mirror's Edge (UU, Left-Handed)
 * a una caja delimitadora en coordenadas de Source Engine (HU, Right-Handed).
 * Como Y se invierte (y_hu = -y_uu * UU_TO_HU), el nuevo min.y es -max.y_uu y el nuevo max.y es -min.y_uu.
 */
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

/**
 * Construye un bloque "solid" de Hammer (.vmf) con sus 6 caras ("side") orientadas en sentido horario
 * vistas desde fuera (convención de planos de Source Engine / vbsp).
 */
function buildVmfSolid(idGenerator, boxHU, material, comment) {
  const solidId = idGenerator.next();
  const { min, max } = boxHU;

  const sides = [
    // Top (+Z)
    {
      plane: `(${min.x} ${max.y} ${max.z}) (${max.x} ${max.y} ${max.z}) (${max.x} ${min.y} ${max.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 -1 0 0] 0.25'
    },
    // Bottom (-Z)
    {
      plane: `(${min.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${max.y} ${min.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 -1 0 0] 0.25'
    },
    // West (-X)
    {
      plane: `(${min.x} ${max.y} ${max.z}) (${min.x} ${min.y} ${max.z}) (${min.x} ${min.y} ${min.z})`,
      uaxis: '[0 1 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    // East (+X)
    {
      plane: `(${max.x} ${max.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${max.z})`,
      uaxis: '[0 1 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    // North (+Y)
    {
      plane: `(${max.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${min.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    // South (-Y)
    {
      plane: `(${max.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${max.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    }
  ];

  const sidesVmf = sides
    .map((s) => {
      const sideId = idGenerator.next();
      return `\t\tside
\t\t{
\t\t\t"id" "${sideId}"
\t\t\t"plane" "${s.plane}"
\t\t\t"material" "${material.toUpperCase()}"
\t\t\t"uaxis" "${s.uaxis}"
\t\t\t"vaxis" "${s.vaxis}"
\t\t\t"rotation" "0"
\t\t\t"lightmapscale" "16"
\t\t\t"smoothing_groups" "0"
\t\t}`;
    })
    .join('\n');

  return `\tsolid
\t{
\t\t"id" "${solidId}"
${sidesVmf}
\t\teditor
\t\t{
\t\t\t"color" "0 180 255"
\t\t\t"visgroupshown" "1"
\t\t\t"visgroupautoshown" "1"
\t\t\t"comments" "${comment}"
\t\t}
\t}`;
}

function main() {
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const uuToHu = manifest.conversion_constants.uu_to_hu;

  let currentId = 1;
  const idGen = { next: () => currentId++ };

  const worldId = idGen.next();
  const solids = [];

  // 1. Crear caja de sellado Skybox alrededor de toda la sección de la azotea para que vbsp no reporte leaks
  const bboxHU = convertBoxUUToHU(manifest.bounding_box_local_uu, uuToHu);
  const pad = 128;
  const wallThick = 32;
  const skyWalls = [
    // Techo Skybox
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad, z: bboxHU.max.z + pad },
      max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad + wallThick }
    },
    // Piso Skybox inferior de seguridad
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad - wallThick },
      max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad, z: bboxHU.min.z - pad }
    },
    // Pared Oeste Skybox
    {
      min: { x: bboxHU.min.x - pad - wallThick, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad },
      max: { x: bboxHU.min.x - pad, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad }
    },
    // Pared Este Skybox
    {
      min: { x: bboxHU.max.x + pad, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad },
      max: { x: bboxHU.max.x + pad + wallThick, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad }
    },
    // Pared Sur Skybox
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad - wallThick, z: bboxHU.min.z - pad },
      max: { x: bboxHU.max.x + pad, y: bboxHU.min.y - pad, z: bboxHU.max.z + pad }
    },
    // Pared Norte Skybox
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.max.y + pad, z: bboxHU.min.z - pad },
      max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad + wallThick, z: bboxHU.max.z + pad }
    }
  ];

  for (const [idx, skyBox] of skyWalls.entries()) {
    solids.push(buildVmfSolid(idGen, skyBox, 'TOOLS/TOOLSSKYBOX', `skybox_seal_${idx}`));
  }

  // 2. Generar cada brush proxy definido en el manifiesto de la azotea Edge_p
  for (const brush of manifest.proxy_brushes) {
    const boxHU = convertBoxUUToHU(brush.bounds_local_uu, uuToHu);
    solids.push(
      buildVmfSolid(idGen, boxHU, brush.source_material, `${brush.id} (${brush.surface_type})`)
    );
  }

  const spawn = manifest.player_spawn.portal_hu;
  const playerStartId = idGen.next();
  const equipId = idGen.next();
  const lightId = idGen.next();

  const vmfContent = `versioninfo
{
\t"editorversion" "400"
\t"editorbuild" "5004"
\t"mapversion" "1"
\t"formatversion" "100"
\t"prefab" "0"
}
visgroups
{
}
viewsettings
{
\t"bSnapToGrid" "1"
\t"bShowGrid" "1"
\t"bShowLogicalGrid" "0"
\t"nGridSpacing" "16"
\t"bShow3DGrid" "0"
}
world
{
\t"id" "${worldId}"
\t"mapversion" "1"
\t"classname" "worldspawn"
\t"skyname" "sky_day01_01"
\t"maxpropscreenwidth" "-1"
\t"detailvbsp" "detail.vbsp"
\t"detailmaterial" "detail/detailsprites"
\t"comment" "Auto-generated Proxy Map for Mirror's Edge Edge_p Rooftop Slice 01"
${solids.join('\n')}
}
entity
{
\t"id" "${playerStartId}"
\t"classname" "info_player_start"
\t"angles" "${spawn.pitch_deg} ${spawn.yaw_deg} ${spawn.roll_deg}"
\t"origin" "${spawn.x} ${spawn.y} ${roundHU(spawn.z + 16)}"
}
entity
{
\t"id" "${equipId}"
\t"classname" "game_player_equip"
\t"spawnflags" "0"
\t"weapon_portalgun" "1"
\t"origin" "${spawn.x} ${spawn.y} ${roundHU(spawn.z + 48)}"
}
entity
{
\t"id" "${lightId}"
\t"classname" "light_environment"
\t"_ambient" "200 220 255 150"
\t"_light" "255 250 240 450"
\t"angles" "0 315 0"
\t"pitch" "-65"
\t"origin" "${spawn.x} ${spawn.y} ${roundHU(bboxHU.max.z - 64)}"
}
`;

  fs.writeFileSync(outputVmfPath, vmfContent, 'utf8');
  console.log(`[OK] Mapa proxy Source (.vmf) generado exitosamente en: ${path.relative(repoRoot, outputVmfPath)}`);
  console.log(`     - Brushes de geometría de Edge_p (Sección 1): ${manifest.proxy_brushes.length}`);
  console.log(`     - Sólidos totales (incluyendo sello skybox): ${solids.length}`);
}

main();

#!/usr/bin/env node
/**
 * generate-stormdrain-vmf.js
 * Generador de mapas Source (.vmf) para el Flujo de Trabajo #2:
 * Capítulo 2: Stormdrain_p — Sección Única: Gran Esclusa Vertical (stormdrain_p_sluice_01).
 *
 * Extiende al generador del Flujo #1 añadiendo:
 * 1. Sólidos en cuña a 45 grados ("wedge_45_plus_x") para catapultas parabólicas (Angled Flings).
 * 2. Entidades interactivas de Portal: Weighted Companion Cube (prop_weighted_cube) y Botón (prop_floor_button).
 * 3. Barreras Emancipation Grill / Fizzlers (prop_portal_cleanser) para confinar la sección única.
 */

const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '../../../..');
const manifestPath = path.join(repoRoot, 'levels', 'stormdrain_p_sluice_01', 'slice_manifest.json');
const outputVmfPath = path.join(repoRoot, 'levels', 'stormdrain_p_sluice_01', 'me_stormdrain_sluice_01.vmf');

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

function buildVmfBoxSolid(idGenerator, boxHU, material, comment) {
  const solidId = idGenerator.next();
  const { min, max } = boxHU;

  const sides = [
    {
      plane: `(${min.x} ${max.y} ${max.z}) (${max.x} ${max.y} ${max.z}) (${max.x} ${min.y} ${max.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 -1 0 0] 0.25'
    },
    {
      plane: `(${min.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${max.y} ${min.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 -1 0 0] 0.25'
    },
    {
      plane: `(${min.x} ${max.y} ${max.z}) (${min.x} ${min.y} ${max.z}) (${min.x} ${min.y} ${min.z})`,
      uaxis: '[0 1 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    {
      plane: `(${max.x} ${max.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${max.z})`,
      uaxis: '[0 1 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    {
      plane: `(${max.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${min.z})`,
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
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
\t\t\t"color" "0 200 180"
\t\t\t"visgroupshown" "1"
\t\t\t"visgroupautoshown" "1"
\t\t\t"comments" "${comment}"
\t\t}
\t}`;
}

/**
 * Construye una cuña/rampa de 5 caras inclinada a 45 grados mirando hacia (+X, +Z).
 * La cara oblicua conecta (min.x, y, max.z) en la cresta superior con (max.x, y, min.z) en la base,
 * permitiendo colocar portales a 45 grados para catapultas parabólicas (Angled Flings).
 */
function buildVmfWedge45PlusX(idGenerator, boxHU, portalMaterial, backMaterial, comment) {
  const solidId = idGenerator.next();
  const { min, max } = boxHU;

  const sides = [
    // 1. Cara Oblicua a 45 grados (+X, +Z) - Superficie PORTALABLE principal
    {
      plane: `(${min.x} ${max.y} ${max.z}) (${max.x} ${max.y} ${min.z}) (${max.x} ${min.y} ${min.z})`,
      material: portalMaterial.toUpperCase(),
      uaxis: '[0 1 0 0] 0.25',
      vaxis: '[0.707107 0 -0.707107 0] 0.25'
    },
    // 2. Base inferior (-Z)
    {
      plane: `(${min.x} ${min.y} ${min.z}) (${max.x} ${min.y} ${min.z}) (${max.x} ${max.y} ${min.z})`,
      material: backMaterial.toUpperCase(),
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 -1 0 0] 0.25'
    },
    // 3. Espalda vertical (-X)
    {
      plane: `(${min.x} ${max.y} ${max.z}) (${min.x} ${min.y} ${max.z}) (${min.x} ${min.y} ${min.z})`,
      material: backMaterial.toUpperCase(),
      uaxis: '[0 1 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    // 4. Lateral Norte (+Y)
    {
      plane: `(${max.x} ${max.y} ${min.z}) (${min.x} ${max.y} ${max.z}) (${min.x} ${max.y} ${min.z})`,
      material: backMaterial.toUpperCase(),
      uaxis: '[1 0 0 0] 0.25',
      vaxis: '[0 0 -1 0] 0.25'
    },
    // 5. Lateral Sur (-Y)
    {
      plane: `(${max.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${min.z}) (${min.x} ${min.y} ${max.z})`,
      material: backMaterial.toUpperCase(),
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
\t\t\t"material" "${s.material}"
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
\t\t\t"color" "255 140 0"
\t\t\t"visgroupshown" "1"
\t\t\t"visgroupautoshown" "1"
\t\t\t"comments" "${comment} [WEDGE_45_DEG]"
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

  // 1. Caja de sellado Skybox alrededor de la Gran Esclusa de Stormdrain_p
  const bboxHU = convertBoxUUToHU(manifest.bounding_box_local_uu, uuToHu);
  const pad = 128;
  const wallThick = 32;
  const skyWalls = [
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad, z: bboxHU.max.z + pad },
      max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad + wallThick }
    },
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad - wallThick },
      max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad, z: bboxHU.min.z - pad }
    },
    {
      min: { x: bboxHU.min.x - pad - wallThick, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad },
      max: { x: bboxHU.min.x - pad, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad }
    },
    {
      min: { x: bboxHU.max.x + pad, y: bboxHU.min.y - pad, z: bboxHU.min.z - pad },
      max: { x: bboxHU.max.x + pad + wallThick, y: bboxHU.max.y + pad, z: bboxHU.max.z + pad }
    },
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.min.y - pad - wallThick, z: bboxHU.min.z - pad },
      max: { x: bboxHU.max.x + pad, y: bboxHU.min.y - pad, z: bboxHU.max.z + pad }
    },
    {
      min: { x: bboxHU.min.x - pad, y: bboxHU.max.y + pad, z: bboxHU.min.z - pad },
      max: { x: bboxHU.max.x + pad, y: bboxHU.max.y + pad + wallThick, z: bboxHU.max.z + pad }
    }
  ];

  for (const [idx, skyBox] of skyWalls.entries()) {
    solids.push(buildVmfBoxSolid(idGen, skyBox, 'TOOLS/TOOLSSKYBOX', `stormdrain_skybox_seal_${idx}`));
  }

  // 2. Generar cada brush del manifiesto (soportando cajas y cuñas a 45 grados)
  for (const brush of manifest.proxy_brushes) {
    const boxHU = convertBoxUUToHU(brush.bounds_local_uu, uuToHu);
    if (brush.shape === 'wedge_45_plus_x') {
      solids.push(
        buildVmfWedge45PlusX(
          idGen,
          boxHU,
          brush.source_material,
          'METAL/METALWALL048B',
          `${brush.id} (${brush.surface_type})`
        )
      );
    } else {
      solids.push(
        buildVmfBoxSolid(idGen, boxHU, brush.source_material, `${brush.id} (${brush.surface_type})`)
      );
    }
  }

  // 3. Generar Entidades (Spawn, Portal Gun, Weighted Companion Cube, Floor Button y Fizzlers)
  const spawn = manifest.player_spawn.portal_hu;
  const cube = manifest.interactive_entities.companion_cube.spawn_portal_hu;
  const button = manifest.interactive_entities.heavy_floor_button;

  const playerStartId = idGen.next();
  const equipId = idGen.next();
  const cubeId = idGen.next();
  const buttonId = idGen.next();
  const relayId = idGen.next();
  const lightId = idGen.next();

  const fizzlerEntities = manifest.interactive_entities.emancipation_grills
    .map((grill) => {
      const fId = idGen.next();
      const fBoxHU = convertBoxUUToHU(grill.bounds_local_uu, uuToHu);
      const fSolid = buildVmfBoxSolid(idGen, fBoxHU, 'EFFECTS/COMBINE_SHIELD001A', grill.id);
      return `entity
{
\t"id" "${fId}"
\t"classname" "${grill.source_classname}"
\t"targetname" "${grill.id}"
\t"StartDisabled" "0"
${fSolid}
}`;
    })
    .join('\n');

  const vmfContent = `versioninfo
{
\t"editorversion" "400"
\t"editorbuild" "5004"
\t"mapversion" "2"
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
\t"mapversion" "2"
\t"classname" "worldspawn"
\t"skyname" "sky_day01_01"
\t"maxpropscreenwidth" "-1"
\t"detailvbsp" "detail.vbsp"
\t"detailmaterial" "detail/detailsprites"
\t"comment" "Auto-generated Proxy Map for Workflow #2: Mirror's Edge Stormdrain_p Vertical Sluice 01"
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
\t"id" "${cubeId}"
\t"classname" "prop_weighted_cube"
\t"targetname" "${manifest.interactive_entities.companion_cube.id}"
\t"CubeType" "1"
\t"SkinType" "0"
\t"origin" "${cube.x} ${cube.y} ${cube.z}"
}
entity
{
\t"id" "${buttonId}"
\t"classname" "prop_floor_button"
\t"targetname" "${button.id}"
\t"origin" "${button.location_portal_hu.x} ${button.location_portal_hu.y} ${button.location_portal_hu.z}"
\tconnections
\t{
\t\t"OnPressed" "${button.target_ue3_kismet_event},Trigger,,0,-1"
\t}
}
entity
{
\t"id" "${relayId}"
\t"classname" "logic_relay"
\t"targetname" "${button.target_ue3_kismet_event}"
\t"origin" "${button.location_portal_hu.x} ${button.location_portal_hu.y} ${roundHU(button.location_portal_hu.z + 64)}"
}
${fizzlerEntities}
entity
{
\t"id" "${lightId}"
\t"classname" "light_environment"
\t"_ambient" "190 215 245 160"
\t"_light" "255 252 245 500"
\t"angles" "0 270 0"
\t"pitch" "-75"
\t"origin" "${spawn.x} ${spawn.y} ${roundHU(bboxHU.max.z - 64)}"
}
`;

  fs.writeFileSync(outputVmfPath, vmfContent, 'utf8');
  console.log(`[OK] Mapa proxy Source del Flujo #2 (.vmf) generado en: ${path.relative(repoRoot, outputVmfPath)}`);
  console.log(`     - Brushes de geometría (incluyendo rampa a 45°): ${manifest.proxy_brushes.length}`);
  console.log(`     - Entidades interactivas: Cubo de Compañía, Botón de Piso -> Kismet (${button.target_ue3_kismet_event}) y ${manifest.interactive_entities.emancipation_grills.length} Fizzlers`);
}

main();

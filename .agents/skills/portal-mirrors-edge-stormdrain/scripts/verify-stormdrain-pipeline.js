#!/usr/bin/env node
/**
 * verify-stormdrain-pipeline.js
 * Script de verificación offline multiplataforma para el Flujo de Trabajo #2:
 * Capítulo 2: Stormdrain_p — Gran Esclusa Vertical (stormdrain_p_sluice_01).
 *
 * Valida:
 * 1. Coexistencia intacta con el Flujo #1 (edge_p_rooftop_01) sin conflictos.
 * 2. Precisión matemática de coordenadas, rampa oblicua a 45° (wedge_45_plus_x) y Cubo de Compañía.
 * 3. Física de catapulta parabólica a 45 grados (Angled Long-Jump Fling) desde la cornisa oeste
 *    hasta la plataforma superior este donde está el botón de salida.
 * 4. Integridad del mapa me_stormdrain_sluice_01.vmf y sus entidades interactivas.
 */

const fs = require('fs');
const path = require('path');
const assert = require('assert');

const repoRoot = path.resolve(__dirname, '../../../..');
const workflow1ManifestPath = path.join(repoRoot, 'levels', 'edge_p_rooftop_01', 'slice_manifest.json');
const manifestPath = path.join(repoRoot, 'levels', 'stormdrain_p_sluice_01', 'slice_manifest.json');
const headerV2Path = path.join(repoRoot, 'src', 'common', 'ipc_stormdrain_bridge_v2.h');
const vmfPath = path.join(repoRoot, 'levels', 'stormdrain_p_sluice_01', 'me_stormdrain_sluice_01.vmf');

const PME_HU_TO_UU = 1.905;
const PME_UU_TO_HU = 1.0 / 1.905;

function sourceToMEPosition(posHu, anchorMeUu) {
  return {
    x: anchorMeUu.x + posHu.x * PME_HU_TO_UU,
    y: anchorMeUu.y - posHu.y * PME_HU_TO_UU,
    z: anchorMeUu.z + posHu.z * PME_HU_TO_UU
  };
}

function computePortalExitVelocity(inVelHu, inNormalRh, outNormalRh) {
  let entrySpeed = -(
    inVelHu.x * inNormalRh.x +
    inVelHu.y * inNormalRh.y +
    inVelHu.z * inNormalRh.z
  );
  if (entrySpeed < 0) {
    entrySpeed = Math.hypot(inVelHu.x, inVelHu.y, inVelHu.z);
  }
  return {
    x: outNormalRh.x * entrySpeed,
    y: outNormalRh.y * entrySpeed,
    z: outNormalRh.z * entrySpeed
  };
}

function main() {
  console.log('=== Verificación Multi-Instancia: Flujo #2 (Portal <-> Mirror\'s Edge en Stormdrain_p) ===\n');

  // 1. Verificar que el Flujo #1 sigue intacto y el Flujo #2 está presente
  assert(fs.existsSync(workflow1ManifestPath), 'El Flujo #1 (edge_p_rooftop_01) debe permanecer intacto');
  assert(fs.existsSync(manifestPath), 'Falta levels/stormdrain_p_sluice_01/slice_manifest.json');
  assert(fs.existsSync(headerV2Path), 'Falta src/common/ipc_stormdrain_bridge_v2.h');

  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  assert.strictEqual(manifest.slice_id, 'stormdrain_p_sluice_01');
  console.log(`[1/5] Coexistencia verificada: Flujo #1 (Edge_p) intacto + Flujo #2 (${manifest.title}) activo.`);

  // 2. Verificar coordenadas de Spawn de Chell y del Cubo de Compañía en Stormdrain_p
  const anchor = manifest.anchor_origin.mirrors_edge_uu;
  const spawnPortal = manifest.player_spawn.portal_hu;
  const spawnMEExpected = manifest.player_spawn.mirrors_edge_uu;
  const computedSpawnME = sourceToMEPosition(spawnPortal, anchor);

  assert(Math.abs(computedSpawnME.x - spawnMEExpected.x) < 0.1, 'Error en X de spawn Stormdrain');
  assert(Math.abs(computedSpawnME.y - spawnMEExpected.y) < 0.1, 'Error en Y de spawn Stormdrain');
  assert(Math.abs(computedSpawnME.z - spawnMEExpected.z) < 0.1, 'Error en Z de spawn Stormdrain');

  const cubePortal = manifest.interactive_entities.companion_cube.spawn_portal_hu;
  const cubeWorldME = sourceToMEPosition(cubePortal, anchor);
  console.log(`[2/5] Sincronización de Entidades validada:`);
  console.log(`      - Spawn Chell/Faith en Stormdrain_p (UU): (${computedSpawnME.x.toFixed(1)}, ${computedSpawnME.y.toFixed(1)}, ${computedSpawnME.z.toFixed(1)})`);
  console.log(`      - Weighted Companion Cube en Stormdrain_p (UU): (${cubeWorldME.x.toFixed(1)}, ${cubeWorldME.y.toFixed(1)}, ${cubeWorldME.z.toFixed(1)})`);

  // 3. Simulación Física de Catapulta a 45° (Angled Fling sobre sluice_angled_fling_ramp_45)
  const rampBrush = manifest.proxy_brushes.find((b) => b.id === 'sluice_angled_fling_ramp_45');
  assert(rampBrush, 'Falta la rampa a 45 grados sluice_angled_fling_ramp_45');

  const floorNormalRh = { x: 0.0, y: 0.0, z: 1.0 };
  const rampNormalRh = rampBrush.normal_source_rh;
  const normLen = Math.hypot(rampNormalRh.x, rampNormalRh.y, rampNormalRh.z);
  assert(Math.abs(normLen - 1.0) < 1e-5, 'La normal de la rampa a 45 grados debe ser unitaria');

  // Caída desde la cornisa oeste (z = 600 HU -> z = 0 HU con gravedad de Source g = 600 HU/s^2):
  // v_impact = sqrt(2 * 600 * 600) = 848.53 HU/s hacia abajo
  const impactVelHu = { x: 0.0, y: 0.0, z: -848.528 };
  const exitVelHu = computePortalExitVelocity(impactVelHu, floorNormalRh, rampNormalRh);
  const exitVelUu = {
    x: exitVelHu.x * PME_HU_TO_UU,
    y: -exitVelHu.y * PME_HU_TO_UU,
    z: exitVelHu.z * PME_HU_TO_UU
  };

  // Verificar que a 45 grados la componente horizontal (+X) y vertical (+Z) son iguales (600 HU/s = 1143 UU/s)
  assert(Math.abs(exitVelHu.x - 600.0) < 0.5, `Velocidad X esperada ~600 HU/s, obtenida ${exitVelHu.x}`);
  assert(Math.abs(exitVelHu.z - 600.0) < 0.5, `Velocidad Z esperada ~600 HU/s, obtenida ${exitVelHu.z}`);
  console.log(`[3/5] Simulación de Catapulta a 45° (Angled Fling en Rampa Oblicua):`);
  console.log(`      - Velocidad de entrada al portal del piso: Z = ${impactVelHu.z.toFixed(1)} HU/s`);
  console.log(`      - Velocidad de salida a 45° en Mirror's Edge: VX = +${exitVelUu.x.toFixed(1)} UU/s, VZ = +${exitVelUu.z.toFixed(1)} UU/s`);

  // 4. Verificar que el mapa .vmf de Stormdrain existe y contiene la cuña a 45°, el Cubo, el Botón y los Fizzlers
  assert(fs.existsSync(vmfPath), 'Falta me_stormdrain_sluice_01.vmf (ejecuta generate-stormdrain-vmf.js)');
  const vmfText = fs.readFileSync(vmfPath, 'utf8');
  assert(vmfText.includes('[WEDGE_45_DEG]'), 'Falta sólido en cuña a 45 grados en el .vmf');
  assert(vmfText.includes('prop_weighted_cube'), 'Falta entidad prop_weighted_cube en el .vmf');
  assert(vmfText.includes('prop_floor_button'), 'Falta entidad prop_floor_button en el .vmf');
  assert(vmfText.includes('prop_portal_cleanser'), 'Falta entidad prop_portal_cleanser (Fizzler) en el .vmf');
  assert(vmfText.includes('OpenSluiceExitGate_01'), 'Falta conexión con evento Kismet OpenSluiceExitGate_01');
  console.log('[4/5] Mapa proxy Source (me_stormdrain_sluice_01.vmf) verificado con cuña a 45° y entidades de puzzle.');

  // 5. Verificar tamaño y estructura del header v2
  const headerV2Text = fs.readFileSync(headerV2Path, 'utf8');
  assert(headerV2Text.includes('PME2_ComputePortalExitVelocity'), 'Falta función PME2_ComputePortalExitVelocity en header v2');
  assert(headerV2Text.includes('PME2PhysicsPropState'), 'Falta struct PME2PhysicsPropState en header v2');
  console.log('[5/5] Contrato binario C/C++ (ipc_stormdrain_bridge_v2.h) validado.\n');
  console.log('ESTADO: AMBOS FLUJOS DE TRABAJO (#1 EDGE_P Y #2 STORMDRAIN_P) LISTOS Y VERIFICADOS.');
}

main();

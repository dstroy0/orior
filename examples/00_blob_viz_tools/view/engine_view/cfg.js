// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The view section of a .cfg: every setting the page has, by the name the .cfg uses, with its type and its range.
// The tracker copies this section into each object untouched and the page opens in it. Applying a .cfg is partial:
// names it does not carry retain their values. A name the scheme does not have, a fraction, a value of the wrong type
// or one outside its range is reported by name and not applied.

EV.VIEW_SCHEME = {
  face: { type: "word", words: ["human", "machine"], fallback: "human" },
  frame: { type: "integer", low: 0, high: 65535, fallback: 0 },
  ghosts: { type: "integer", low: 0, high: 8, fallback: 0 },
  ghost_fade: { type: "integer", low: 0, high: 224, fallback: 48 },
  turn: { type: "integer", low: 0, high: 359, fallback: 30 },
  tilt: { type: "integer", low: -89, high: 89, fallback: 35 },
  zoom: { type: "integer", low: 0, high: 1024, fallback: 0 },
  pan_x: { type: "integer", low: -1048576, high: 1048576, fallback: 0 },
  pan_y: { type: "integer", low: -1048576, high: 1048576, fallback: 0 },
  z_scale: { type: "integer", low: 2, high: 16, fallback: 8 },
  spread: { type: "integer", low: 0, high: 2048, fallback: 0 },
  body: { type: "word", words: ["smooth", "voxels", "centroids"], fallback: "smooth" },
  smooth_radius: { type: "integer", low: 1, high: 8, fallback: 3 },
  smooth_opacity: { type: "integer", low: 1, high: 255, fallback: 16 },
  walls: { type: "switch", fallback: true },
  wall_tone: { type: "integer", low: 0, high: 255, fallback: 110 },
  wall_opacity: { type: "integer", low: 1, high: 255, fallback: 220 },
  slice_follow: { type: "switch", fallback: true },
  map: { type: "word", words: ["projection", "slice"], fallback: "projection" },
  palette: { type: "word", words: ["lineage", "okabe_ito", "cell", "volume"], fallback: "okabe_ito" },
  min_voxels: { type: "integer", low: 0, high: 65536, fallback: 0 },
  light: { type: "switch", fallback: true },
  lights: { type: "lights", fallback: [{ turn: 300, tilt: 50, strength: 210 }, { turn: 130, tilt: 20, strength: 90 }] },
  body_opacity: { type: "integer", low: 0, high: 255, fallback: 160 },
  // Nothing of the microscope at 0, all of it at 255, and the two of them together anywhere between.
  slide: { type: "integer", low: 0, high: 255, fallback: 0 },
  links: { type: "switch", fallback: true },
  tracks: { type: "switch", fallback: false },
  edges: { type: "switch", fallback: true },
  correct: { type: "switch", fallback: true },
  branched: { type: "switch", fallback: true },
  wrong: { type: "switch", fallback: true },
  no_link: { type: "switch", fallback: true },
  only_chosen: { type: "switch", fallback: false },
  glow: { type: "switch", fallback: true },
  labels: { type: "switch", fallback: true },
  ride: { type: "switch", fallback: false },
  slice: { type: "switch", fallback: true },
  slice_z: { type: "integer", low: -1, high: 65535, fallback: -1 },
  window_low: { type: "integer", low: 0, high: 65535, fallback: 0 },
  window_high: { type: "integer", low: 1, high: 65535, fallback: 1024 },
  rate: { type: "integer", low: 0, high: 60, fallback: 2 },
  step: { type: "integer", low: 1, high: 64, fallback: 1 },
  loop: { type: "switch", fallback: true },
  sheer: { type: "integer", low: 20, high: 100, fallback: 90 },
  text_size: { type: "integer", low: 80, high: 200, fallback: 100 },
  theme: { type: "word", words: ["dark", "light"], fallback: "dark" },
  vsync: { type: "switch", fallback: true },
  panels: { type: "words", words: ["counts", "dims", "lineage", "legend", "review", "machine"], fallback: ["counts"] },
  chosen: { type: "integers", low: 0, high: 4294967294, fallback: [] },
};

// The whole .cfg: the object's own sections as the tracker wrote them, with the page's view in place of its view.
EV.cfgText = (objectCfgText, view) => {
  let base = {};
  try {
    base = objectCfgText ? JSON.parse(objectCfgText) : {};
  } catch (error) {
    base = {};
  }
  const cfg = { scheme: "cell_tracking.cfg", version: 1, track: base.track || {}, input: base.input || {}, output: base.output || {}, view };
  return JSON.stringify(cfg, null, 2) + "\n";
};

// The specimen the input section names: species, voxel size z y x and membrane thickness, in picometers. An object
// written before the input section carried them reads as unknown, with a membrane of 0.
EV.specimen = (cfgText) => {
  let input = {};
  try {
    input = (JSON.parse(cfgText || "{}").input) || {};
  } catch (error) {
    input = {};
  }
  const whole = (value) => Number.isInteger(value) && value >= 0;
  const voxel = Array.isArray(input.voxel_pm) && input.voxel_pm.length === 3 && input.voxel_pm.every(whole) ? input.voxel_pm : [0, 0, 0];
  return {
    species: typeof input.species === "string" ? input.species : "",
    voxel_pm: voxel,
    membrane_pm: whole(input.membrane_pm) ? input.membrane_pm : 0,
  };
};

// Reads a .cfg's text for the page: checks the scheme, version and every section name, and passes back the view.
EV.parseCfg = (text) => {
  let cfg;
  try {
    cfg = JSON.parse(text);
  } catch (error) {
    return { error: [`that is not JSON: ${error.message}`] };
  }
  if (!cfg || typeof cfg !== "object" || Array.isArray(cfg)) {
    return { error: ["a .cfg is one object"] };
  }
  const error = [];
  if (cfg.scheme !== undefined && cfg.scheme !== "cell_tracking.cfg") {
    error.push("scheme: always cell_tracking.cfg");
  }
  if (cfg.version !== undefined && cfg.version !== 1) {
    error.push("version: this page reads version 1");
  }
  for (const name of Object.keys(cfg)) {
    if (!["scheme", "version", "track", "input", "output", "view"].includes(name)) {
      error.push(`${name}: a .cfg has scheme, version, track, input, output and view`);
    }
  }
  return { error, view: cfg.view };
};

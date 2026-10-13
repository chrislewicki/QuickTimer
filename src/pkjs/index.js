/* Quick Timer phone side (spec §13.3). ES5 only. */
var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var keys = require('message_keys');

var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

/* Fixed AppMessage keys for the watch; must match APPMSG_KEY_* in src/c/config.h. */
var KEY_PRESET_COUNT = 900;
var KEY_EXIT_AFTER = 901;
var KEY_PRESET_SECONDS = 910;

var MAX_SLOTS = 12;
var MAX_SECONDS = 86399;
var MULTIPLIERS = { s: 1, m: 60, h: 3600 };

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) {
    return;
  }

  var settings = clay.getSettings(e.response);
  var list = [];

  for (var i = 0; i < MAX_SLOTS; i++) {
    var rawValue = settings[keys.PresetValue + i];
    var unit = settings[keys.PresetUnit + i];
    var v = parseFloat(rawValue);
    if (!(v > 0) || !MULTIPLIERS.hasOwnProperty(unit)) {
      continue;
    }
    var secs = Math.round(v * MULTIPLIERS[unit]);
    if (secs < 1 || secs > MAX_SECONDS) {
      continue;
    }
    if (list.indexOf(secs) === -1) {
      list.push(secs);
    }
  }

  /* Numeric sort; the default sort() compares as text. */
  list.sort(function (a, b) { return a - b; });

  var dict = {};
  dict[KEY_PRESET_COUNT] = list.length;
  for (var j = 0; j < list.length; j++) {
    dict[KEY_PRESET_SECONDS + j] = list[j];
  }
  dict[KEY_EXIT_AFTER] = settings[keys.ExitAfterStart] ? 1 : 0;

  console.log('Quick Timer: sending ' + list.length + ' presets: ' + list.join(', '));
  Pebble.sendAppMessage(dict, function () {
    console.log('Quick Timer: settings sent');
  }, function (err) {
    console.log('Quick Timer: settings failed: ' + JSON.stringify(err));
  });
});

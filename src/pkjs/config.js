/* Clay settings page for Quick Timer (spec §13.1). ES5 only. */

var DEFAULT_VALUES = ['1', '2', '3', '4', '5', '10', '15', '20', '30', '1', '2', ''];
var DEFAULT_UNITS = ['m', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'h', 'h', 'm'];

var presetItems = [
  {
    type: 'heading',
    defaultValue: 'Quick Launch presets'
  },
  {
    type: 'text',
    defaultValue: 'Shown bottom to top, smallest first. Leave a value empty or 0 to drop ' +
                  'that slot. Duplicates are merged. Longest allowed: 23 h 59 min 59 s.'
  }
];

for (var i = 0; i < 12; i++) {
  presetItems.push({
    type: 'input',
    messageKey: 'PresetValue[' + i + ']',
    id: 'pv' + i,
    label: 'Preset ' + (i + 1),
    defaultValue: DEFAULT_VALUES[i],
    attributes: {
      type: 'number',
      min: 0,
      step: 'any',
      placeholder: 'empty'
    }
  });
  presetItems.push({
    type: 'select',
    messageKey: 'PresetUnit[' + i + ']',
    id: 'pu' + i,
    label: 'Unit',
    defaultValue: DEFAULT_UNITS[i],
    options: [
      { label: 'seconds', value: 's' },
      { label: 'minutes', value: 'm' },
      { label: 'hours', value: 'h' }
    ]
  });
}

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Quick Timer'
  },
  {
    type: 'section',
    items: presetItems
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Behavior'
      },
      {
        type: 'toggle',
        messageKey: 'ExitAfterStart',
        label: 'Close the app after starting a timer from Quick Launch',
        defaultValue: false
      }
    ]
  },
  {
    type: 'button',
    id: 'resetPresets',
    defaultValue: 'Reset presets to defaults'
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];

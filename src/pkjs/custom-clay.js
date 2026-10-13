/* Runs inside the Clay settings page (spec §13.2). Clay serializes this function
   with toString(), so it must not use anything defined outside its own body. ES5 only. */
module.exports = function (minified) {
  var clayConfig = this;
  var DEFAULT_VALUES = ['1', '2', '3', '4', '5', '10', '15', '20', '30', '1', '2', ''];
  var DEFAULT_UNITS = ['m', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'h', 'h', 'm'];

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var resetButton = clayConfig.getItemById('resetPresets');
    if (!resetButton) {
      return;
    }
    resetButton.on('click', function () {
      for (var i = 0; i < 12; i++) {
        var valueItem = clayConfig.getItemById('pv' + i);
        var unitItem = clayConfig.getItemById('pu' + i);
        if (valueItem) {
          valueItem.set(DEFAULT_VALUES[i]);
        }
        if (unitItem) {
          unitItem.set(DEFAULT_UNITS[i]);
        }
      }
    });
  });
};

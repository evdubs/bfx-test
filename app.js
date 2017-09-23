'use strict';

const addon = require('./build/Release/bfx_test');

setInterval(function(){
  var i = Math.random();
  var j = Math.random();
  
  var bid = Math.min(i, j);
  var ask = Math.max(i, j);

  addon.BidAskAvg({ "bid" : bid, "ask" : ask }, function(s) { 
    console.log(`Asynchronously received ${s} from addon.Run as the average between ${bid} and ${ask}`);
  });
}, 2000);


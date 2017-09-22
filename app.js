'use strict';

const addon = require('./build/Release/addon');

var i = 0;
setInterval(function(){
  const buffer = Buffer.from(i.toString());

  addon.Run(buffer, buffer.length, function(s) { 
    console.log(`Asynchronously received ${s} from addon.Run`);
  });
  
  i++;
}, 2000);


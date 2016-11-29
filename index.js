'use strict'

// this is a test file

const unrar = require('bindings')('node-unrar');
const path = require('path');
const fs = require('fs');

const realpath = path.resolve('test.cbr');
if (!fs.existsSync(realpath)) {
  console.log('file does not exists');
}
else {
  unrar.processArchive({ path: realpath, openMode: 0 }, (err, value) => {
    if (err) {
      console.log(err.message);
      console.log(value);
    }
    else {
      console.log(value);
    }
  });
}

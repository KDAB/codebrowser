#!/usr/bin/env node
/*
 * This script can be used to generate the compile_commands.json file.
 * You need node.js to run this.
 * This script works as a fake compiler, whose partial name between "woboq_" and ".js" is used as the internal real compiler.
 * So you can symbolic link woboq_cc.js(cc) to woboq_clang++.js(clang++) or what ever compiler you need.
 * Example:
 * CC=/path/to/woboq_cc.js CXX=/path/to/woboq_c++.js ./configure
 * rm -rf $PWD/compile_commands.json
 * JSON_DIR=$PWD make -j1
 */
var fs = require('fs'),
    path = require('path'),
    child_process = require('child_process');

var jsonfile = process.env.JSON_DIR;
if (jsonfile) {
  jsonfile = path.join(jsonfile, 'compile_commands.json');
}

var argvs = process.argv;
argvs.shift();  // remove "node"
var scriptName = argvs.shift();  // remove "woboq_xx.js" and extract the "xx"
var compiler = scriptName.match(/.*woboq_(.*?)\.js/)[1];
if (!compiler) {
  console.error("Can not extract the compiler name from script name " + scriptName);
  process.exit(1);
}

var srcfile = path.resolve(argvs.slice(-1).join());
fs.access(srcfile, fs.R_OK, (accerr) => {
  if (jsonfile && !accerr) {
    fs.readFile(jsonfile, function(err, data) {
      var compile_commands = [];
      if (!err) {
        try {
          compile_commands = JSON.parse(data);
        } catch(e) {
          // ignore it, later we will overwrite it
        }
      }
      compile_commands.push({
        "directory" : process.cwd(),
        "command" : [compiler].concat(argvs).join(' '),
        "file" : srcfile
      });
      fs.writeFileSync(jsonfile, JSON.stringify((compile_commands)));
    });
  }
});

child_process.spawnSync(compiler, argvs, { stdio: 'inherit' });

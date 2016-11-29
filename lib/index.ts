'use strict'

// this is a test file
import * as Path from 'path';
import * as Fs from 'fs';

const unrar = require('bindings')('node-unrar');


export enum OpenMode {
  LIST = 0,
  EXTRACT = 1
}

export interface RarOptions {
  openMode?: OpenMode,
  password?: string;
}

export interface RarResult {
  name: string;
  files: string[];
}

export type RarCallback = (err: any, result: RarResult) => void;

function promisify (bind: any, method: any, args: any): Promise<any> {

  return new Promise((resolve, reject) => {

    const callback = (err: any, result: any) => {

      if (err) {
          return reject(err);
      }

      return resolve(result);
    };

    method.apply(bind, args ? args.concat(callback) : [callback]);
  });
};

export function processArchive (path: string, options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void {

  const realpath = Path.resolve(path);

  if (typeof options === 'function') {
    cb = options;
  }

  if (!cb) {
    return promisify(this, processArchive, [path, options]);
  }

  if (!Fs.existsSync(realpath)) {
    const error = new Error(`${realpath}: No such file or directory`);
    return cb(error, null);
  }

  let opts: any = {
    openMode: 0,
    path: realpath
  };

  if (typeof options === 'object') {
    // <any>Object to avoid error
    opts = (<any>Object).assign({}, opts, options);
  }

  unrar.processArchive(opts, cb);
}


export function list (path: string, cb?: RarCallback) {

  return processArchive(path, cb);
};
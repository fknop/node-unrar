import * as Path from 'path';
import * as Fs from 'fs';
import { promisify } from './promisify';

const unrar = require('bindings')('node-unrar');

export enum OpenMode {
  List = 0,
  Extract = 1
}

export interface RarOptions {
  openMode?: OpenMode,
  password?: string;
  dest?: string;
  humanResults?: boolean;
}

export interface HRFiles {
  [key: string]: HRFiles;
}

export interface RarResult {
  name: string;
  files: string[]|HRFiles;
}

export interface FileEntry {
  [key: string]: string|FileEntry[];
}

export type RarCallback = (err: any, result: RarResult) => void;

const defaults: RarOptions = {
  openMode: 0
};

function overrideDefaults (...options: any[]) {

  return (<any>Object).assign({}, defaults, ...options);
}


function getFilesMap (files: string[]): HRFiles {

  const map: HRFiles = {};

  files.forEach((file: string) => {

    const split = file.split(Path.sep);
    let previous: HRFiles = map;
    let current: string;
    for (let i = 0; i < split.length; ++i) {

      current = split[i];
      if (!previous[current]) {
        previous[current] = {};
      }

      previous = previous[current];
    }
  });

  return map;
}


export function processFiles (files: string[]): HRFiles {

  return getFilesMap(files);
}

export function processArchive (path: string, options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void {

  const realpath = Path.resolve(path);

  if (!cb && typeof options === 'function') {
    cb = options;
  }

  if (!cb) {
    return promisify(this, processArchive, [path, options]);
  }


  if (!Fs.existsSync(realpath)) {
    const error = new Error(`${realpath}: No such file or directory`);
    return cb(error, null);
  }

  let opts: any = overrideDefaults(options, { path: realpath });

  if (opts.dest) {
    opts.dest = Path.resolve(opts.dest);
  }

  unrar.processArchive(opts, (err: any, result: RarResult) => {
    if (err) {
      cb(err, null);
    }
    else {
      if (opts.humanResults) {
        result.files = processFiles(<string[]>result.files);
      }

      cb(null, result);
    }
  });
}


export function list (path: string,  options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void {

  if (!cb && typeof options === 'function') {
    cb = options;
  }

  if (!cb) {
    return promisify(this, processArchive, [path, options]);
  }

  const opts = overrideDefaults(options, { openMode: OpenMode.List });
  return processArchive(path, opts, cb);
};


export function extract (path: string, options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void {

  if (!cb && typeof options === 'function') {
    cb = options;
  }

  if (!cb) {
    return promisify(this, processArchive, [path, options]);
  }

  const opts = overrideDefaults(options, { openMode: OpenMode.Extract });
  return processArchive(path, opts, cb);
}

export function processArchiveSync (path: string, options?: RarOptions): RarResult {

  const realpath = Path.resolve(path);
  const opts: any = overrideDefaults(options, { path: realpath });
  return unrar.processArchive(opts);
}

export function extractSync (path: string, options?: RarOptions): RarResult {
  return processArchiveSync(path, overrideDefaults(options, { openMode: OpenMode.Extract }));
}

export function listSync (path: string, options?: RarOptions): RarResult {
  return processArchiveSync(path, overrideDefaults(options, { openMode: OpenMode.List }));
}
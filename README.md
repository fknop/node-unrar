# node-rar

Native NodeJS rar addon. 

This library is inspired by https://github.com/davidcroda/node-rar and the original library.
I needed a native unrar addon that worked with the new versions of node.
I also updated the addon to be able to process an archive `asynchronously`.

## Install 

```
npm install @fknop/node-unrar
```

It's published under the @fknop scope to avoid using a name like `node-unrar2` or something like that.

## API

### `OpenMode`

* `List` = 0,
* `Extract` = 1

### `RarResult`

* name: the archive name 
* files: an array of files in the archive (see `RarOptions.humanResults` for more details)

### `RarOptions`

* `openMode`: the archive open mode
* `dest`: the destination directory for extraction (ignored in list mode).
* `password`: the archive password 
* `humanResults`: (not yet implemented) - instead of an array of raw string names, display the files as a tree

Without `humanResults`:

```javascript
{
  name: 'archive.rar',
  files: [
    'dir',
    'dir/file1',
    'dir/file2',
    'dir/file3',
    'dir/dir2/file4',
    'dir/dir2/file5',
    'file'
  ]
}
```

With `humanResults`:

```javascript
{
  name: 'archive.rar',
  files: [
    'dir': [
      'file1',
      'file2',
      'file3',
      'dir2': [
        'file4',
        'file5'
      ]
    ],
    'file'
  ]
}
```

### `RarCallback`

`RarCallback` is a simple node callback `(err, results) => { ... }`.

### `processArchive(path: string, options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void`

Process an archive asynchronously. By default, it lists files.

* `path`: the path of the archive 
* `options`: The options, or the callback if no options are needed
* `cb`: the callback if options are needed

If no callback is provided, a promise will be returned.

### `list(path: string, options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void`

Alias for `processArchive` with `OpenMode.List`.

### `extract(path: string, options?: RarOptions|RarCallback, cb?: RarCallback): Promise<RarResult>|void`

Alias for `processArchive` with `OpenMode.Extract`.

### `processArchiveSync(path: string, options?: RarOptions): RarResult`

Process an archive synchronously. By default, it lists files.
If an error occurs, it will throw an exception.

### `listSync(path: string, options?: RarOptions): RarResult`

Alias for `processArchiveSync` with `OpenMode.List`.

### `extractSync(path: string, options?: RarOptions): RarResult`

Alias for `processArchiveSync` with `OpenMode.Extract`.

## Todo

* `humanResults` option (my priority)
* Tests
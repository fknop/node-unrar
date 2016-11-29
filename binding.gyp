{
  "targets": [
   {
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
      ],
      "target_name": "node-unrar",
      "sources": [ "src/binding.cc" ],
      "defines": [
        "_FILE_OFFSET_BITS=64",
        "_LARGEFILE_SOURCE",
        "RARDLL"
      ],
      "dependencies": [
        "./vendor/unrar/unrar.gyp:unrar"
      ]
    }
  ]
}

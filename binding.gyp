{
  "targets": [
    {
      "target_name": "bfx_test",
      "sources": [ "zmq_backend.cc" ],
      "libraries": [ "-lzmq" ],
      "include_dirs" : ["<!(node -e \"require('nan')\")"]
    }
  ]
}


{
  "targets": [
    {
      "target_name": "addon",
      "sources": [ "zmq_backend.cc" ],
      "libraries": [ "-lzmq" ],
      "include_dirs" : ["<!(node -e \"require('nan')\")"]
    }
  ]
}


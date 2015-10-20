{
  "targets": [
    {
      "target_name": "i2c",
      "include_dirs": [
      	"node_modules/nan",
      	"<!(node -e \"require('nan')\")" 
      ],
      "sources": [ "src/i2c.cc" ]
    }
  ]
}
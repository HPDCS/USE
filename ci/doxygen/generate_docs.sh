#!/bin/bash 

if [[ $# -eq 1 ]]; then
  doxygen doxy.conf
fi

python -m coverxygen --xml-dir ../../docs/xml/ --src-dir ../../src --output doc-coverage.info --kind all --scope all --format json
python process-doc-info.py
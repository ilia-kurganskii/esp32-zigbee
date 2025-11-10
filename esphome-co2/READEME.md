somehow works
docker run --rm --privileged -v "${PWD}":/config --device=/dev/ttyACM0 -it ghcr.io/esphome/esphome run co2_scd40.yaml
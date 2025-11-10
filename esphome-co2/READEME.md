somehow works
```
docker run --rm --privileged -v "${PWD}":/config --device=/dev/ttyACM0 -it ghcr.io/esphome/esphome run co2_scd40.yaml
```


5v GND 6 7

docker run --rm --privileged -v $PWD:/project -w /project --device=/dev/ttyACM0 -it espressif/idf 
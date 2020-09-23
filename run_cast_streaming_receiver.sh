
#!/usr/bin/env sh

echo "Running receiver in foreground..."
./out/Default/cast_receiver lo0 -v -x -p ./cast_streaming.key -s ./cast_streaming.crt &
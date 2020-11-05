# [libcast] Cast Streaming Control Protocol (CSCP)

## What is it?

CSCP is the standardized and modernized implement of the Castv2 Mirroring
Control Protocol, a legacy API implemented both inside of
[Chrome's Mirroring Service](https://source.chromium.org/chromium/chromium/src/+/master:components/mirroring/service/receiver_response.h;l=75?q=receiverresponse%20&ss=chromium%2Fchromium%2Fsrc) and other Google products that communicate with
Chrome, such as Chromecast. This API handles session control messaging, such as
managing OFFER/ANSWER exchanges, getting status and capability information from
the receiver, and exchanging RPC messaging for handling media remoting.

## What's in this folder?
The `streaming_schema.json` file in this directory contains a
[JsonSchema](https://json-schema.org/) for the validation of control messaging
defined in the Cast Streaming Control Protocol. This includes comprehensive
rules for message definitions as well as valid values and ranges.

The `validate_examples.sh` runs the control protocol against a wide variety of
example files in this directory, one for each type of supported message in CSCP.
In order to see what kind of validation this library provides, you can modify
these example files and see what kind of errors this script presents.

NOTE: this script uses [`ajv-cli`](https://www.npmjs.com/package/ajv-cli), which
must be installed by Node.js.

For example, if a new app type is added that we don't understand, we should get a failure validation like so:

```
-> % ./validate_examples.sh
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/answer.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/capabilities_response.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/get_capabilities.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/get_status.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/offer.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/rpc.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/streaming_examples/status_response.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/app_examples/app_availability.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/app_examples/get_app_availability.json valid
/usr/local/src/openscreen/cast/streaming/control_protocol/app_examples/launch.json invalid
[
  {
    keyword: 'enum',
    dataPath: '.supportedAppTypes[1]',
    schemaPath: '#/properties/supportedAppTypes/items/enum',
    params: { allowedValues: [Array] },
    message: 'should be equal to one of the allowed values'
  }
]
/usr/local/src/openscreen/cast/streaming/control_protocol/app_examples/stop.json valid
```
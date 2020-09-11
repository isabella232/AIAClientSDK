As a non-trivial software system, the AIA SDK has some known limitations and problems. The following is a list of problems we are aware of.

## Offline Alerts are not supported

The client is able to handle alerts when it is connected to the AIA service. However, it does not currently support playing alerts when disconnected, as specified by [the AIA spec](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-alerts.html).

This will be added in a subsequent release.

## Certification

Please note that this SDK has not gone through the [official AVS certification process](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/product-testing-overview.html) yet. 

## Announcements

We have not tested [Alexa announcements](https://developer.amazon.com/en-US/docs/alexa/acm/announcements-acm.html) yet.

# /Presence/SetWatch response

Build **204489** | `sts` | `s2c` | `/Presence/SetWatch`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 200 OK

Accepted authenticated request.

```xml
<Reply></Reply>
```

### 403 Forbidden

When the session or login policy guard rejects the request.

```xml
<Error code="4" text="Forbidden"/>
```

### Notification

Notification sent separately after the reply; this notification has no status line or sequence.

```xml
<Message><Status>online</Status><Aliases></Aliases><OnlineTimes></OnlineTimes><AppData></AppData><Channels></Channels><Groups></Groups><Contacts></Contacts><UserId>{user_id}</UserId><UserName>{display_name_xml}</UserName><NewBeginning></NewBeginning></Message>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.
- The empty reply is followed by a notification containing l:{body_bytes}, CRLF CRLF and the XML body. The notification is also sent when the reply is suppressed.

## Related messages

- [sts c2s presence-setwatch](../../../204489/sts/c2s/presence-setwatch.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s presence-setwatch](../../../204489/sts/c2s/presence-setwatch.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

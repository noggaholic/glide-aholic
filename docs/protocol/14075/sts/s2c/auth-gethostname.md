# /Auth/GetHostname response

Build **14075** | `sts` | `s2c` | `/Auth/GetHostname`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 200 OK

Successful hostname query.

```xml
<Reply><Hostname>cligate.101.ncplatform.net</Hostname><Local>true</Local></Reply>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s auth-gethostname](../../../14075/sts/c2s/auth-gethostname.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s auth-gethostname](../../../14075/sts/c2s/auth-gethostname.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

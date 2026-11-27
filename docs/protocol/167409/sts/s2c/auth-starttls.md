# /Auth/StartTls response

Build **167409** | `sts` | `s2c` | `/Auth/StartTls`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 400 Success

Before the TLS handshake, when the request is accepted.

```xml
<Error server="1001" module="4" line="262"/>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s auth-starttls](../../../167409/sts/c2s/auth-starttls.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s auth-starttls](../../../167409/sts/c2s/auth-starttls.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

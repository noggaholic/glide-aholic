# /Auth/LoginFinish response

Build **49284** | `sts` | `s2c` | `/Auth/LoginFinish`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 403 Forbidden

When the session or login policy guard rejects the request.

```xml
<Error code="4" text="Forbidden"/>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s auth-loginfinish](../../../49284/sts/c2s/auth-loginfinish.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s auth-loginfinish](../../../49284/sts/c2s/auth-loginfinish.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

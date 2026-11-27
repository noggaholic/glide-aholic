# /Auth/RequestGameToken response

Build **49284** | `sts` | `s2c` | `/Auth/RequestGameToken`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 200 OK

Game token issued for the active local session.

```xml
<Reply><Token>{game_token}</Token></Reply>
```

### 403 Forbidden

When the session or login policy guard rejects the request.

```xml
<Error code="4" text="Forbidden"/>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s auth-requestgametoken](../../../49284/sts/c2s/auth-requestgametoken.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s auth-requestgametoken](../../../49284/sts/c2s/auth-requestgametoken.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

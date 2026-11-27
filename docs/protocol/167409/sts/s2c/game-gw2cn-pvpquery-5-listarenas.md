# /Game.gw2cn.PvpQuery.5/ListArenas response

Build **167409** | `sts` | `s2c` | `/Game.gw2cn.PvpQuery.5/ListArenas`

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


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s game-gw2cn-pvpquery-5-listarenas](../../../167409/sts/c2s/game-gw2cn-pvpquery-5-listarenas.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s game-gw2cn-pvpquery-5-listarenas](../../../167409/sts/c2s/game-gw2cn-pvpquery-5-listarenas.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

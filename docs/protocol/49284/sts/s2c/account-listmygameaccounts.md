# /Account/ListMyGameAccounts response

Build **49284** | `sts` | `s2c` | `/Account/ListMyGameAccounts`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 200 OK

Authenticated account-list request.

```xml
<Reply type="array"><Row><Alias>{alias_xml}</Alias></Row></Reply>
```

### 403 Forbidden

When the session or login policy guard rejects the request.

```xml
<Error code="4" text="Forbidden"/>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s account-listmygameaccounts](../../../49284/sts/c2s/account-listmygameaccounts.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s account-listmygameaccounts](../../../49284/sts/c2s/account-listmygameaccounts.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

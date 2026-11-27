# /Auth/StartSsoLogin response

Build **49284** | `sts` | `s2c` | `/Auth/StartSsoLogin`

Envelope: [STS](../../../transport.md#sts-envelope).

## Responses

### 200 OK

Successful local password login.

```xml
<Reply><UserId>{user_id}</UserId><ResumeToken>{resume_token}</ResumeToken><EmailVerified>1</EmailVerified></Reply>
```

### 403 Forbidden

When the session or login policy guard rejects the request.

```xml
<Error code="4" text="Forbidden"/>
```

### 400 Bad Request

Required login XML fields are invalid.

```xml
<Error code="4" text="Bad Request"/>
```


## Notes

- The response envelope is STS/1.0 {status} {status_text}, s:{sequence}R and l:{body_bytes}. A missing request sequence suppresses the reply.

## Related messages

- [sts c2s auth-startssologin](../../../49284/sts/c2s/auth-startssologin.md) (reply): Request handled by this response branch.

## Referenced by

- [sts c2s auth-startssologin](../../../49284/sts/c2s/auth-startssologin.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

[Build index](../../README.md)

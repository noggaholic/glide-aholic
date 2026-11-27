# /Auth/Resume

Build **167409** | `sts` | `c2s` | `/Auth/Resume`

Envelope: [STS](../../../transport.md#sts-envelope).

## XML body fields

| Tag | Required | Maximum decoded bytes | Meaning |
|---|:---:|---:|---|
| `ResumeToken` | Yes | 36 | Local resume token. |

## Notes

- Requests use P {path} STS/1.0 with l: body length and an optional s: sequence. See Transport for exact framing and limits.
- A successful /Sts/Connect must precede this request.
- Requires an active TLS connection under this profile. Missing TLS receives 403 Forbidden.
- Malformed or missing XML values receive 400 Bad Request. Invalid credentials/token or a conflicting active session receive 403 Forbidden.
- A reply is sent only when the request includes a sequence. The request body is consumed in full in either case.
- XML values are found by exact tag spelling, reject duplicate opening tags, decode only the five named XML entities and require valid UTF-8. Other body fields are not established by this schema.

## Related messages

- [sts s2c auth-resume](../../../167409/sts/s2c/auth-resume.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

## Referenced by

- [sts s2c auth-resume](../../../167409/sts/s2c/auth-resume.md) (reply): Request handled by this response branch.

[Build index](../../README.md)

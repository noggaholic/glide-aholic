# /Auth/StartTls

Build **14505** | `sts` | `c2s` | `/Auth/StartTls`

Envelope: [STS](../../../transport.md#sts-envelope).

## Notes

- Requests use P {path} STS/1.0 with l: body length and an optional s: sequence. See Transport for exact framing and limits.
- A successful /Sts/Connect must precede this request.
- Requires a sequence, no existing TLS session and no logged-in session. Replies with the special 400 Success body, then begins TLS. Repeating or invalid sequencing closes the connection.
- TLS uses the password/SRP path (June2012 variant).
- A reply is sent only when the request includes a sequence. The request body is consumed in full in either case.
- XML values are found by exact tag spelling, reject duplicate opening tags, decode only the five named XML entities and require valid UTF-8. Other body fields are not established by this schema.

## Related messages

- [sts s2c auth-starttls](../../../14505/sts/s2c/auth-starttls.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

## Referenced by

- [sts s2c auth-starttls](../../../14505/sts/s2c/auth-starttls.md) (reply): Request handled by this response branch.

[Build index](../../README.md)

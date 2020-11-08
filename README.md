# GPS Unit


Authorized domains: "wodeewa.com"

## Auth backend: google

Web client ID: `4...np.apps.googleusercontent.com`


## Project setup
```
npm install
```

### Compiles and hot-reloads for development
```
npm run serve
```

### Compiles and minifies for production
```
npm run build
```

### Lints and fixes files
```
npm run lint
```

### Customize configuration
See [Configuration Reference](https://cli.vuejs.org/config/).


src/assets/Roboto.css: `https://fonts.googleapis.com/css?family=Roboto:100,300,400,500,700,900`
src/assets/materialdesignicons.min.css: `https://cdn.jsdelivr.net/npm/@mdi/font@latest/css/materialdesignicons.min.css`

The `idToken` returned by Firebase is a JWT, with the following content:

Header:
`{
  "alg": "RS256",
  "kid": "0a...a91",
  "typ": "JWT"
}`

Payload:
`{
  "iss": "accounts.google.com",
  "azp": "4...np.apps.googleusercontent.com",
  "aud": "4...np.apps.googleusercontent.com",
  "sub": "10...87",
  "email": "gabor.simon75@gmail.com",
  "email_verified": true,
  "at_hash": "P...Q",
  "iat": 15...01,
  "exp": 15...01
}`

Signature: ...

`Authorization: Bearer <token>`

# Fitness Studio


Authorized domains: "wodeewa.com"

## Auth backend: google

Web client ID: `4...6-esjb4i5m1el65gd2p4hbjenu62mf3tnp.apps.googleusercontent.com`


## Project setup
```
npm install
npx cap update android
# npx cap copy android
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
  "kid": "0a7dc12664590c957ffaebf7b6718297b864ba91",
  "typ": "JWT"
}`

Payload:
`{
  "iss": "accounts.google.com",
  "azp": "4...6-esjb4i5m1el65gd2p4hbjenu62mf3tnp.apps.googleusercontent.com",
  "aud": "4...6-esjb4i5m1el65gd2p4hbjenu62mf3tnp.apps.googleusercontent.com",
  "sub": "106223510888703572487",
  "email": "gabor.simon75@gmail.com",
  "email_verified": true,
  "at_hash": "PYCs2Oslpy7t-bZukIzCrQ",
  "iat": 1598698901,
  "exp": 1598702501
}`

Signature: ...

`Authorization: Bearer <token>`

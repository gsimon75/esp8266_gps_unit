module.exports = {
    root: true,
    env: {
        node: true,
    },
    "extends": [
        "plugin:vue/essential",
        "eslint:recommended",
    ],
    parserOptions: {
        parser: "babel-eslint"
    },
    rules: {
        "no-console": process.env.NODE_ENV === "production" ? "warn" : "off",
        "no-debugger": process.env.NODE_ENV === "production" ? "warn" : "off",
        "no-unused-vars": "warn",
        "no-undef": "warn",
        "no-irregular-whitespace": "off",
        "no-constant-condition": "off", // while (true) ...
    },
};
// vim: set sw=4 ts=4 indk= et:

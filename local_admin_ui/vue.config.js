const HtmlWebpackPlugin = require("html-webpack-plugin")
const HtmlWebpackInlineSourcePlugin = require("html-webpack-inline-source-plugin");

module.exports = {
    devServer: {
        disableHostCheck: true,
    },
    css: {
        extract: false,
        loaderOptions: {
            sass: {
                prependData: `@import "@/styles/_variables.sass"` // empty file
            },
            scss: {
                prependData: `@import "@/styles/_variables.scss"; @import "@/styles/_mixins.scss";`
            }
        }
    },
    chainWebpack: config => {
        config.plugins.delete("prefetch");
        config.plugins.delete("preload");
        config.module.rule("images").use("url-loader").loader("url-loader").tap(options => Object.assign(options, { limit: 131072 }));
        //config.plugin("VuetifyLoaderPlugin").tap(args => [{ }]);
        config.plugin("html").tap(args => [{
            filename: "index.html",
            template: "public/index.html",
            inlineSource: ".(js|css)$" // embed all javascript and css inline
        }]);
        config.plugin("htmlinline").use(HtmlWebpackInlineSourcePlugin, []);
    },
    outputDir: "www",
    publicPath: "",
    filenameHashing: process.env.NODE_ENV !== "production",
    transpileDependencies: [
        "vuetify",
    ],
    productionSourceMap: false,
    pluginOptions: {
        foo: {
          // plugins can access these options as `options.pluginOptions.foo`.
        },
    }
}
// vim: set sw=4 ts=4 indk= et:

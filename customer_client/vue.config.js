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
    configureWebpack: {
        devtool: "eval-cheap-source-map",
    },
    chainWebpack: config => {
        config.plugins.delete("prefetch");
        config.module.rule("images").use("url-loader").loader("url-loader").tap(options => Object.assign(options, { limit: 131072 }));
        config.plugin("html").tap(args => {
            args[0].favicon = "./src/assets/favicon.png";
            return args;
        });
        config.plugin("VuetifyLoaderPlugin").tap(args => [{
        }]);
    },
    outputDir: "www",
    publicPath: "",
    filenameHashing: true,
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

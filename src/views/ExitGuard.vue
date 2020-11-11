<template>
    <div class="exit-guard">
        <span>Either starting or stopping...</span>
    </div>
</template>

<script>
import { Plugins } from '@capacitor/core';
const { App } = Plugins;

export default {
    name: "ExitGuard",
    created: function() {
        if (this.$store.state.is_started) {
            console.log("Exiting the app now");
            if (typeof cordova !== "undefined") {
                App.exitApp();
            }
        }
        else {
            var def = "/site_map";
            if (!this.$store.getters.is_logged_in) {
                def = "/signin";
            }
            if (this.$route.fullPath !== def) {
                this.$router.push(def);
            }
        }
    },
}
</script>
// vim: set sw=4 ts=4 indk= et:

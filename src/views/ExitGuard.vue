<template>
    <div class="exit-guard">
        <span>Either starting or stopping...</span>
    </div>
</template>

<script>

export default {
    name: "ExitGuard",
    created: function() {
        console.log("ExitGuard created");
        if (this.$store.state.is_started) {
            console.log("Exiting the app now");
            if (typeof cordova !== "undefined") {
                navigator.app.exitApp();
            }
        }
        else {
            console.log("Proceed to map");
            this.$store.commit("mark_started", true);
            this.$router.push("/site_map");
        }
    },
}
</script>
// vim: set sw=4 ts=4 indk= et:

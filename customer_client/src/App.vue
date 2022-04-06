<template>
    <v-app id="app">
        <v-app-bar color="primary" short dense dark>
            <v-app-bar-nav-icon @click.stop="drawer=!drawer"/>
            <v-toolbar-title>{{ $store.state.app_bar_info }}</v-toolbar-title>
            <v-spacer/>
            <span class="mr-2">{{ $store.getters.auth.name }}</span>
            <v-btn to="/home" text><v-img class="shrink mr-2" contain src="./assets/logo.png" width="24px" height="24px"/></v-btn>
        </v-app-bar>

        <v-navigation-drawer v-model="drawer" app clipped>
             <v-list dense nav>
                <v-list-item to="/take_scooter">
                    <v-list-item-icon><v-icon>fas fa-biking</v-icon></v-list-item-icon>
                    <v-list-item-content>Take scooter</v-list-item-content>
                </v-list-item>

                <v-list-item to="/return_scooter">
                    <v-list-item-icon><v-icon>fas fa-bicycle</v-icon></v-list-item-icon>
                    <v-list-item-content>Return scooter</v-list-item-content>
                </v-list-item>

                <v-list-item to="/site_map">
                    <v-list-item-icon><v-icon>fas fa-map-marked-alt</v-icon></v-list-item-icon>
                    <v-list-item-content>Site Map</v-list-item-content>
                </v-list-item>

                <v-list-item to="/account">
                    <v-list-item-icon><v-icon>fas fa-user</v-icon></v-list-item-icon>
                    <v-list-item-content>Account</v-list-item-content>
                </v-list-item>

                <v-list-item to="/sign_in" v-if="!$store.getters.have_auth_token">
                    <v-list-item-icon><v-icon>fas fa-sign-in-alt</v-icon></v-list-item-icon>
                    <v-list-item-content>Sign in</v-list-item-content>
                </v-list-item>

             </v-list>
        </v-navigation-drawer>

        <v-main style="height: calc(100vh - 48px - 56px); overflow-y: scroll; z-index:0;">
            <router-view/>
        </v-main>

         <v-bottom-navigation color="primary" dark>
            <v-btn to="/take_scooter" text :disabled="!$store.state.near_station || ($store.state.near_station.ready <= 0)"><v-icon>fas fa-biking</v-icon></v-btn>
            <v-btn to="/return_scooter" text :disabled="!$store.state.near_station || ($store.state.near_station.free <= 0) || ($store.getters.scooters_in_use.length == 0)"><v-icon>fas fa-bicycle</v-icon></v-btn>
            <v-btn to="/" text :disabled="true"><v-icon>fas fa-home</v-icon></v-btn>
            <v-btn to="/site_map" text><v-icon>fas fa-map-marked-alt</v-icon></v-btn>
         </v-bottom-navigation>
    </v-app>
</template>

<script>

export default {
    name: "app",
    data: () => ({
        drawer: null,
    }),
    created: function() {
        console.log("App created");
        this.$router.push("/sign_in");
    },
};
</script>

<style lang="scss">
@import "~vuetify/src/styles/main.sass";

    :root {
        --app-bar-height: 48px;
        --bottom-nav-height: 56px;
        --main-height: calc(100vh - var(--app-bar-height) - var(--bottom-nav-height));
    }

    #app {
        font-family: Avenir, Helvetica, Arial, sans-serif;
        -webkit-font-smoothing: antialiased;
        -moz-osx-font-smoothing: grayscale;
        color: #2c3e50;
    }

    #app > .v-app-bar {
        height: var(--app-bar-height);
        flex-grow: 0;
    }

    #app > .v-main {
        height: var(--main-height);
        overflow-y: scroll;
        z-index:0;
    }

    #app > .v-bottom-navigation {
        height: var(--bottom-bar-height);
        flex-grow: 0;
    }

    .row-0 {
        display: flex;
        margin-right: 0;
        margin-left: 0;
        > .col, > [class*="col-"] {
            padding: 0;
        }
    }

    .row-1 {
        @extend .row-0;
        height: 8.3333333333%;
    }

    .row-2 {
        @extend .row-0;
        height: 16.6666666667%;
    }

    .row-3 {
        @extend .row-0;
        height: 25%;
    }

    .row-4 {
        @extend .row-0;
        height: 33.3333333333%;
    }

    .row-5 {
        @extend .row-0;
        height: 41.6666666667%;
    }

    .row-6 {
        @extend .row-0;
        height: 50%;
    }

    .row-7 {
        @extend .row-0;
        height: 58.3333333333%;
    }

    .row-8 {
        @extend .row-0;
        height: 66.6666666667%;
    }

    .row-9 {
        @extend .row-0;
        height: 75%;
    }

    .row-10 {
        @extend .row-0;
        height: 83.3333333333%;
    }

    .row-11 {
        @extend .row-0;
        height: 91.6666666667%;
    }

    .row-12 {
        @extend .row-0;
        height: 100%;
    }

    .background-energy {
        background-color: map-get($lime, "lighten-4");
    }
    .color-energy {
        color: map-get($lime, "darken-1");
    }

    .background-protein {
        background-color: map-get($orange, "lighten-4");
    }
    .color-protein {
        background-color: map-get($orange, "base");
    }

    .background-carbs {
        background-color: map-get($purple, "lighten-4");
    }
    .color-carbs {
        background-color: map-get($purple, "base");
    }
    .color-carbs-sub {
        background-color: map-get($purple, "lighten-2");
    }

    .background-fat {
        background-color: map-get($red, "lighten-4");
    }
    .color-fat {
        background-color: map-get($red, "base");
    }
    .color-fat-sub {
        background-color: map-get($red, "lighten-2");
    }

    .background-water {
        background-color: map-get($blue, "lighten-4");
    }
    .color-water {
        background-color: map-get($blue, "base");
    }

    .background-vitamin {
        background-color: map-get($teal, "lighten-4");
    }
    .color-vitamin {
        background-color: map-get($teal, "lighten-2");
    }

    .background-mineral {
        background-color: map-get($green, "lighten-4");
    }
    .color-mineral {
        background-color: map-get($green, "lighten-2");
    }

</style>
// vim: set sw=4 ts=4 indk= et:

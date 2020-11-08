<template>
    <div class="settings">
        <v-container fluid>
            
            <v-row dense>
                <v-col cols="12"> <v-combobox v-model="language" label="Language" :items="S4DataSource.languages" item-text="name" placeholder="" dense></v-combobox> </v-col>
            </v-row>
            
            <v-row dense>
                <v-col cols="12"> <v-combobox v-model="starting_route" label="Opening screen" :items="$router.options.routes" item-text="name" placeholder="" dense></v-combobox> </v-col>
            </v-row>
            
        </v-container>
    </div>
</template>

<script>
import S4DataSource from "@/plugins/S4DataSource";

export default {
    name: "SettingsPage",
    data () {
        return {
            S4DataSource,
            language: null,

            starting_route: null,
            open_screen: "Settings",
        };
    },
    watch: {
        language: function (val, oldval) {
            console.log("language changed from " + JSON.stringify(oldval) + " to " + JSON.stringify(val));
        },
    },
    methods: {
    },
    mounted: function () {
        this.$store.state.app_bar_info = "Settings";
    },
    beforeDestroy: function () {
        this.$store.state.app_bar_info = "...";
    },
}

</script>

<style lang="scss">
    label.v-label:not(.v-label--active) {
        font-size: 1rem;
    }

    div[role="combobox"] {
        margin-top: 12px;

        input[type="text"] {
            margin-top: 6px;
        }
    }
</style>

<style lang="scss" scoped>
	.v-input {
        font-size: 1rem;
    }

</style>

// vim: set sw=4 ts=4 et indk= :

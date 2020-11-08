<template>
    <v-container class="sign-in">
        <v-row dense>
            <v-col>
                <h1>Sign in with</h1>
            </v-col>
        </v-row>

        <v-row dense>
            <v-col>
                <v-btn @click="sign_in_with_google">Google</v-btn>
            </v-col>
        </v-row>

        <v-row v-if="$store.getters.auth.id_token != ''" dense>
            <v-col>
                <v-textarea label="ID Token" :value="$store.getters.auth.id_token"></v-textarea>
            </v-col>
        </v-row>

        <v-row v-if="$store.getters.auth.id_token != ''" dense>
            <v-col cols="4">
                <span>Name: {{ $store.getters.auth.name }}</span>
            </v-col>
            <v-col cols="8">
                <span>Email: {{ $store.getters.auth.email }}</span>
            </v-col>
        </v-row>

        <v-row v-if="$store.getters.auth.id_token != ''" dense>
            <v-col cols="4">
                <span>Provider: {{ $store.getters.auth.provider_id }}</span>
            </v-col>
            <v-col cols="8">
                <span>UID: {{ $store.getters.auth.uid }}</span>
            </v-col>
        </v-row>

        <v-row v-if="$store.getters.auth.id_token != ''" dense>
            <v-col>
                <img :src="$store.getters.auth.photo_url">
            </v-col>
        </v-row>

    </v-container>
</template>

<script>

export default {
    name: "SignInPage",
    methods: {
        sign_in_with_google: function () {
            this.$store.dispatch("sign_in_with_google").then(user => {
                    this.$store.commit("logged_in", user);
                    this.$router.replace("/");
            });
        },
    },
}

</script>

<style>
    .el-row {
        margin-bottom: 20px;
        &:last-child {
            margin-bottom: 0;
        }
    }
</style>

// vim: set sw=4 ts=4 et indk= :

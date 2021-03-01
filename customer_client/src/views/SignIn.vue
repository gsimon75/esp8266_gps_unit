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

        <v-row dense>
            <v-col>
                <v-btn @click="showing_emailpwd_dialog = true">Email+password</v-btn>
            </v-col>
        </v-row>

        <!--
        Dialog for obtaining email+password login credentials
        -->
        <v-dialog v-model="showing_emailpwd_dialog" max-width="90vw">
            <v-card outlined>
                <v-card-title>
                    Sign-in with email and password
                </v-card-title>
                <v-card-text>
                    <v-row dense>
                        <v-text-field v-model="auth_email" label="E-mail"/>
                    </v-row>
                    <v-row dense>
                        <v-text-field v-model="auth_pwd" label="Password"
                            :append-icon="show_password ? 'fas fa-eye' : 'fas fa-eye-slash'" :type="show_password ? 'text' : 'password'" @click:append="show_password = !show_password"/>
                    </v-row>
                    <v-row dense>
                        <span>(For this service, NOT the one for your email account!)</span>
                    </v-row>
                    <v-row dense>
                        <v-checkbox v-model="auth_signup" label="Sign up"/>
                    </v-row>
                    <v-row dense v-if="auth_signup">
                        <v-text-field v-model="auth_displayname" label="Name"/>
                    </v-row>
                </v-card-text>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn class="mb-8" color="success" @click="sign_in_with_email_pwd">Sign {{ auth_signup ? "up" : "in" }}</v-btn>
                    </v-row>
                </v-card-actions>
            </v-card>
        </v-dialog>

    </v-container>
</template>

<script>

export default {
    name: "SignInPage",
    data () {
        return {
            showing_emailpwd_dialog: false,
            auth_email: "",
            auth_pwd: "",
            auth_signup: false,
            show_password: false,
            auth_displayname: "",
        };
    },
    methods: {
        proceed: function () {
            console.log("Proceed to map");
            this.$store.commit("mark_started", true);
            //this.$router.push("/site_map");
            this.$router.push("/account");
        },
        sign_in_with_google: function () {
            this.$store.dispatch("sign_in_with_google").then(() => {
                this.proceed();
            });
        },
        sign_in_with_email_pwd: function () {
            this.showing_emailpwd_dialog = false;
            this.$store.dispatch("sign_in_with_email_pwd", {email: this.auth_email, pwd: this.auth_pwd, signup: this.auth_signup, displayName: this.auth_displayname})
                .then(this.proceed)
                .catch(err => alert("Sign-in failed: " + err));
        },
    },
    created: function() {
        console.log("ExitGuard created");
        if (this.$store.state.is_started) {
            console.log("Exiting the app now");
            if (typeof cordova !== "undefined") {
                navigator.app.exitApp();
            }
        }
        else if (this.$store.getters.have_auth_token) {
            console.log("Check if we already have a session");
            this.$store.dispatch("sign_in_to_backend", this.$store.state.auth.id_token).then(this.proceed);
        }
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

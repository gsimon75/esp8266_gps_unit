<template>
    <div class="take-scooter">
        <!--
        Simulate: scanning QR-Code from the scooter
        -->
        <div>
            <div class="circle green white--text ma-auto">
                <i class="fa fa-motorcycle fa-10x"></i>
            </div>
        </div>

        <div>
            <h1>Scooter #41</h1>
        </div>
        <div class="d-flex flex-row justify-space-between">
            <h1>Battery:</h1>
            <h1>1:25</h1>
        </div>
        <div>
            <v-btn class="big-button" x-large block elevation="8" color="green" @click="take_scooter(41)"><h2>Take it</h2></v-btn>
        </div>

        <!--
        Alert for mocking the QR code scanning step
        -->
        <v-dialog v-model="scanning_qr_code" max-width="90vw">
            <v-card outlined>
                <v-card-title>
                    Scanning QR code
                </v-card-title>
                <v-card-text>
                    (Now only simulated.)
                    <img src="../assets/mock_qr_code.png">
                </v-card-text>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn class="mb-8" color="success" @click="qr_scan_done">Simulate scan</v-btn>
                    </v-row>
                </v-card-actions>
            </v-card>
        </v-dialog>

    </div>
</template>

<script>
import { simulation_enabled } from "../modules/geoshapes";

export default {
    name: "TakeScooter",
    data () {
        return {
            scanning_qr_code: true,
        };
    },
    computed: {
    },
    methods: {
        qr_scan_done: function () {
            this.scanning_qr_code = false;
        },
        take_scooter: function () {
            simulation_enabled[0] = true;
            this.$router.back();
        },

        debug: function() {
            alert("yadda");
        },
    },
    created: function() {
        console.log("TakeScooter created");
        simulation_enabled[0] = false;
    },
}

</script>

<style lang="scss" scoped>
@import "~vuetify/src/styles/main.sass";

    .take-scooter {
        padding-left: 10%;
        padding-right: 10%;
        height: 100%;
        display: flex;
        flex-direction: column;
        justify-content: space-evenly;
    }

    .v-row {
        margin-bottom: 20px;
        &:last-child {
            margin-bottom: 0;
        }
    }

    .v-card__title {
        word-break: normal;
    }

    .big-button {
        height: 7em;
    }

    div.circle {
        shape-outside: circle(50%);
        display: flex;
        justify-content: center;
        align-items: center;
        border-radius: 50%;
        opacity: 0.5;
    }
	div.circle {
		width: 40vw;
		height: 40vw;
	}
	div.circle i {
		font-size: 20vw;
	}
</style>

// vim: set sw=4 ts=4 et indk= :

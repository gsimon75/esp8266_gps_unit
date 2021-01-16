<template>
    <div class="return-scooter">
        <div>
            <div class="circle red white--text ma-auto">
                <i class="fa fa-motorcycle fa-10x"></i>
            </div>
        </div>

        <v-list>
            <v-list-item v-for="(id, idx) in $store.getters.scooters_in_use" :key="id" :class="(idx & 1) ? 'odd' : 'even'">
                <v-list-item-content>
                    <span class="text-left">Scooter #{{ id }}</span>
                    <span class="text-right">0:23</span>
                </v-list-item-content>
                <v-list-item-action>
                    <v-btn x-small icon @click="return_scooter(id)">
                        <v-icon small color="error">fas fa-times</v-icon>
                    </v-btn>
                </v-list-item-action>
            </v-list-item>
            <v-divider/>
        </v-list>

    </div>
</template>

<script>

export default {
    name: "ReturnScooter",
    data () {
        return {
        };
    },
    methods: {
        return_scooter: function (id) {
            this.$store.commit("return_scooter", id);
            if (this.$store.getters.scooters_in_use.length == 0) {
                this.$router.back();
            }
        },
    },
}

</script>

<style lang="scss" scoped>
@import "~vuetify/src/styles/main.sass";

    .return-scooter {
        padding-left: 10%;
        padding-right: 10%;
        display: flex;
        flex-direction: column;
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

    .v-list-item.even {
        background-color: rgba(255, 255, 0, 0.2);
    }

    .v-list-item.odd {
        background-color: rgba(0, 255, 255, 0.2);
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

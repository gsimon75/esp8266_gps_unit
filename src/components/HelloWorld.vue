<template>
    <v-container>
        <v-row class="text-center">
            <v-col cols="12">
                <h1>{{ msg }}, {{ $store.getters.auth.name }}</h1>
            </v-col>
        </v-row>
    </v-container>
</template>

<script>
export default {
    name: 'HelloWorld',
    props: {
        msg: String
    },
    data: () => ({
    }),
}
</script>

<!-- Add "scoped" attribute to limit CSS to this component only -->
<style scoped>
h3 {
    margin: 40px 0 0;
}
ul {
    list-style-type: none;
    padding: 0;
}
li {
    display: inline-block;
    margin: 0 10px;
}
a {
    color: #42b983;
}
</style>
// vim: set sw=4 ts=4 indk= et:

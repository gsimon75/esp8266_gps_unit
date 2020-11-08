<template>
    <div class="home">
        <HelloWorld msg="Welcome to Your Vue.js App"/>
    </div>
</template>

<script>
// @ is an alias to /src
import HelloWorld from "@/components/HelloWorld.vue"

export default {
    name: "Home",
    components: {
        HelloWorld
    }
}
</script>
// vim: set sw=4 ts=4 indk= et:

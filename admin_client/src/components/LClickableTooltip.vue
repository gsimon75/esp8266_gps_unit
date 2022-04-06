<script>
import { tooltip, DomEvent } from "leaflet";

const findRealParent = firstVueParent => {
    while (firstVueParent && (firstVueParent.mapObject === undefined)) {
        firstVueParent = firstVueParent.$parent;
    }
    return firstVueParent;
};

export default {
    name: "LClickableTooltip",
    props: {
        content: {
            type: String,
            default: null,
            custom: true
        }
    },
    mounted() {
        const options = { permanent: true, };
        this.mapObject = tooltip(options);
        DomEvent.on(this.mapObject, this.$listeners);

        const el = this.content || this.$el;
        this.mapObject.setContent(el);

        const self = this;
        el.addEventListener("click", function() {
            if (self.$listeners.click) {
                self.$listeners.click(self);
            }
        });
        el.style.pointerEvents = "auto";

        this.parentContainer = findRealParent(this.$parent);
        this.parentContainer.mapObject.bindTooltip(this.mapObject);

        this.$nextTick(() => {
            this.$emit("ready", this.mapObject);
        });
    },
    methods: {
        setContent (newVal) {
            if (this.mapObject && newVal !== null && newVal !== undefined) {
                this.mapObject.setContent(newVal);
            }
        }
    },
    render (h) {
        if (this.$slots.default) {
            return h('div', this.$slots.default);
        }
        return null;
    },
    beforeDestroy() {
        if (this.parentContainer) {
            if (this.parentContainer.unbindTooltip) {
                this.parentContainer.unbindTooltip();
            }
            else if (this.parentContainer.mapObject && this.parentContainer.mapObject.unbindTooltip) {
                this.parentContainer.mapObject.unbindTooltip();
            }
        }
    },
};
</script>
// vim: set sw=4 ts=4 indk= et:


class S4Date {
    static ONE_DAY = 24*60*60*1000;
    static daynames = [];

    static monthnames = [];

    static install (Vue, options) { // eslint-disable-line no-unused-vars
    }

    static twodigit (x) {
        // This is a lame shit. But you know what? With a language that doesn't have a tool for sprintf("%02d", x), I just don't care.
        // All the alternatives are regex-heavy reinventions of the wheel, so ... tell me an easier and more effective way that doesn't have a megabyte large dependency footprint.
        return (x < 10) ? "0" + x : x;
    }

    static time2str (minutes, use_12_hours = false) { // eslint-disable-line no-unused-vars
        if (!use_12_hours) {
            return this.twodigit(0 | (minutes / 60)) + ":" + this.twodigit(minutes % 60);
        }
        // https://en.wikipedia.org/wiki/12-hour_clock#Confusion_at_noon_and_midnight
        // Pick your favorite one and tell me why this code is wrong. Wait, don't tell...
        else if (minutes < 60) {
            return "12:" + this.twodigit(minutes % 60) + " AM";
        }
        else if (minutes < 12 * 60) {
            return this.twodigit(0 | (minutes / 60)) + ":" + this.twodigit(minutes % 60) + " AM";
        }
        else if (minutes < 13 * 60) {
            return "12:" + this.twodigit(minutes % 60) + " PM";
        }
        else {
            return this.twodigit(0 | ((minutes / 60) - 12)) + ":" + this.twodigit(minutes % 60) + " PM";
        }
    }

    static date2str (d) {
        return d.toISOString().substring(0, 10);
    }

    constructor (ts) {
        if (typeof ts === "undefined") {
            const now = new Date();
            ts = Date.UTC(now.getFullYear(), now.getMonth(), now.getDate());
        }
        const d = new Date(ts);
        this.timestamp = ts;
        this.date = d;
        this.str = this.constructor.date2str(d);
        this.year = d.getFullYear();
        this.month = d.getMonth();
        this.day = d.getDate();
        this.weekday = d.getDay();
    }

    same_year (other) {
        return this.year == other.year;
    }

    same_month (other) {
        return this.same_year(other) && (this.month == other.month);
    }

    same_week (other, weekStarts = 0) { // eslint-disable-line no-unused-vars
        const this_dow = (this.weekday - weekStarts + 7) % 7;
        const other_dow = (other.weekday - weekStarts + 7) % 7;
        return this.same_month(other) && ((this.day - this_dow) == (other.day - other_dow));
    }

    same_day (other) {
        return this.same_month(other) && (this.day == other.day);
    }

    add_day (delta) {
        return new S4Date(this.timestamp + delta * this.constructor.ONE_DAY);
    }

    add_week (delta) {
        return new S4Date(this.timestamp + delta * 7*this.constructor.ONE_DAY);
    }

    add_month (delta) {
        var year = this.year;
        var month = this.month + delta;
        if (month < 0) {
            year += 0|((month - 11) / 12);
            month = 12 + (month % 12);
        }
        else if (12 <= month) {
            year += 0|(month / 12);
            month = month % 12;
        }

        // set the new start date
        return new S4Date(Date.UTC(year, month, 1));
    }

}

for (let i = 1; i <= 7; i++) {
    const d = new Date(1970, 1, i);
    S4Date.daynames[d.getDay()] = {
        short: d.toLocaleDateString(undefined, {weekday: "short"}),
        long: d.toLocaleDateString(undefined, {weekday: "long"}),
    }
}

for (let i = 0; i <= 11; i++) {
    const d = new Date(1970, i, 1);
    S4Date.monthnames[d.getMonth()] = {
        short: d.toLocaleDateString(undefined, {month: "short"}),
        long: d.toLocaleDateString(undefined, {month: "long"}),
    }
}

export default S4Date;
// vim: set sw=4 ts=4 indk= et:

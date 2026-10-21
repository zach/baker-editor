// The suite's entry in VS Code's extension host: runs extension.test.js with mocha.
const path = require("path");
const Mocha = require("mocha");

exports.run = () => new Promise((resolve, reject) => {
    const mocha = new Mocha({ui: "tdd", timeout: 60000, color: false});
    mocha.addFile(path.join(__dirname, "extension.test.js"));
    mocha.run((failures) => failures ? reject(new Error(`${failures} tests failed`)) : resolve());
});

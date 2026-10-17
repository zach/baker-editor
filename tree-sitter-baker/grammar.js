/**
 * Baker data files (.dta): arrays, commands, brackets, and the atoms between them. The
 * preprocessor's directives (#include, #ifdef, ...) are tokens like any other here; Baker
 * itself acts on them while reading.
 */
const delimiter = /[\s(){}\[\];"']/;

module.exports = grammar({
  name: 'baker',

  extras: $ => [/\s/, $.comment, $.block_comment],

  word: $ => $.symbol,

  rules: {
    source_file: $ => repeat($._node),

    _node: $ => choice(
      $.array, $.command, $.bracket,
      $.number, $.string, $.quoted_symbol, $.variable, $.directive, $.symbol,
    ),

    // (tag values...): data. Its tag is its first node, when that is a symbol.
    array: $ => seq('(', repeat($._node), ')'),
    // {function args...}: script, run when its value is read.
    command: $ => seq('{', repeat($._node), '}'),
    // [NAME nodes...] at the top of a file defines a macro; [name] in a method is a
    // property of $this.
    bracket: $ => seq('[', repeat($._node), ']'),

    number: _ => token(prec(2, /-?(0[xX][0-9A-Fa-f]+|\d+\.?\d*([eE][-+]?\d+)?|\.\d+([eE][-+]?\d+)?)/)),
    // Two escapes, \n and \q (a double quote), neither of which holds a quote.
    string: _ => token(seq('"', /[^"]*/, '"')),
    quoted_symbol: _ => token(seq("'", /[^']*/, "'")),
    variable: _ => token(seq('$', /[^\s(){}\[\];"']+/)),
    directive: _ => token(prec(1, /#[A-Za-z_][A-Za-z0-9_]*/)),
    symbol: _ => token(/[^\s(){}\[\];"'$]+/),

    comment: _ => token(seq(';', /.*/)),
    block_comment: _ => token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/')),
  },
});

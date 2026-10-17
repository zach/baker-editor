; Baker data files.

(comment) @comment
(block_comment) @comment
(string) @string
(quoted_symbol) @string.special.symbol
(number) @number
(variable) @variable
(directive) @keyword.directive

; An array's tag, a command's function, a bracket's macro or property name.
(array . (symbol) @tag)
(array . (quoted_symbol) @tag)
(command . (symbol) @function.call)
(bracket . (symbol) @constant.macro)

["(" ")" "{" "}" "[" "]"] @punctuation.bracket

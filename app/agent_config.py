"""Bounded JSON/JSONC member edits that retain unrelated settings and comments."""
from dataclasses import dataclass, field
import json
import re


@dataclass
class Node:
    start: int
    end: int
    value: object
    members: dict = field(default_factory=dict)


class Document:
    def __init__(self, text, comments=False):
        if len(text.encode('utf-8')) > 1024 * 1024:
            raise ValueError('Client configuration exceeds 1 MiB')
        self.text, self.comments, self.at, self.count = text, comments, 0, 0
        self.root = self.parse(0)
        self.space()
        if self.at != len(text) or not isinstance(self.root.value, dict):
            raise ValueError('Expected a client configuration object')

    def space(self):
        while self.at < len(self.text):
            if self.text[self.at].isspace():
                self.at += 1
            elif self.comments and self.text.startswith('//', self.at):
                end = self.text.find('\n', self.at)
                self.at = len(self.text) if end < 0 else end
            elif self.comments and self.text.startswith('/*', self.at):
                end = self.text.find('*/', self.at + 2)
                if end < 0:
                    raise ValueError('Unterminated configuration comment')
                self.at = end + 2
            else:
                break

    def parse(self, depth):
        self.count += 1
        if depth > 32 or self.count > 100000:
            raise ValueError('Excessive client configuration nesting')
        self.space()
        start = self.at
        if self.at >= len(self.text):
            raise ValueError('Incomplete client configuration')
        opening = self.text[self.at]
        if opening in '{[':
            self.at += 1
            closing = '}' if opening == '{' else ']'
            value = {} if opening == '{' else []
            members = {}
            self.space()
            while self.at < len(self.text) and self.text[self.at] != closing:
                key_start = self.at
                if opening == '{':
                    key = self.parse(depth + 1)
                    if not isinstance(key.value, str) or key.value in value:
                        raise ValueError('Duplicate or invalid configuration key')
                    self.space()
                    if self.text[self.at:self.at + 1] != ':':
                        raise ValueError('Missing configuration colon')
                    self.at += 1
                item = self.parse(depth + 1)
                if opening == '{':
                    value[key.value] = item.value
                    members[key.value] = (key_start, item, None)
                else:
                    value.append(item.value)
                self.space()
                if self.text[self.at:self.at + 1] != ',':
                    break
                comma = self.at
                if opening == '{':
                    members[key.value] = (key_start, item, comma)
                self.at += 1
                self.space()
                if self.text[self.at:self.at + 1] == closing and not self.comments:
                    raise ValueError('Trailing comma in JSON configuration')
            if self.text[self.at:self.at + 1] != closing:
                raise ValueError('Incomplete configuration object/array')
            self.at += 1
            return Node(start, self.at, value, members)
        try:
            value, consumed = json.JSONDecoder().raw_decode(self.text[self.at:])
        except ValueError as error:
            raise ValueError('Invalid configuration JSON') from error
        if isinstance(value, float) and (value != value or abs(value) == float('inf')):
            raise ValueError('Invalid numeric configuration value')
        self.at += consumed
        return Node(start, self.at, value)

    def member(self, path):
        node = self.root
        for key in path:
            if not isinstance(node.value, dict):
                raise ValueError('Client server configuration is not an object')
            if key not in node.members:
                return None
            node = node.members[key][1]
        return node

    def raw(self, path):
        node = self.member(path)
        return None if node is None else self.text[node.start:node.end]

    def set(self, path, value, *, remove=False):
        if not path:
            raise ValueError('Expected a named configuration entry')
        parent = self.member(path[:-1])
        if parent is None:
            if remove:
                return self.text
            missing = next(i for i in range(len(path)) if self.member(path[:i + 1]) is None)
            nested = value
            for key in reversed(path[missing + 1:]):
                nested = {key: nested}
            return self.set(path[:missing + 1], nested)
        if not isinstance(parent.value, dict):
            raise ValueError('Client server configuration is not an object')
        key = path[-1]
        text = self.text
        newline = '\r\n' if '\r\n' in text else '\n'
        line_start = text.rfind('\n', 0, parent.start) + 1
        indentation = re.match(r'[ \t]*', text[line_start:parent.start]).group()
        indent = indentation + '  '
        encoded = json.dumps(value, ensure_ascii=False, indent=2).replace('\n', newline + indent)
        edits = []
        if key in parent.members:
            start, node, comma = parent.members[key]
            if not remove:
                # Derive the actual entry indentation for a replaced multiline value.
                entry_start = text.rfind('\n', 0, start) + 1
                prefix = re.match(r'[ \t]*', text[entry_start:start]).group()
                encoded = json.dumps(value, ensure_ascii=False, indent=2).replace('\n', newline + prefix)
                edits.append((node.start, node.end, encoded))
            else:
                edits.append((start, node.end, ''))
                if comma is not None:
                    edits.append((comma, comma + 1, ''))
                else:
                    items = list(parent.members)
                    index = items.index(key)
                    if index:
                        previous = parent.members[items[index - 1]][2]
                        if previous is not None:
                            edits.append((previous, previous + 1, ''))
        elif not remove:
            if parent.members:
                last = list(parent.members.values())[-1]
                if last[2] is None:
                    edits.append((last[1].end, last[1].end, ','))
            edits.append((parent.end - 1, parent.end - 1,
                          newline + indent + json.dumps(key) + ': ' + encoded + newline + indentation))
        # Inserts at the same offset are applied in reverse construction order.
        # In a compact object the final value and closing brace are adjacent.
        for _, (start, end, replacement) in sorted(enumerate(edits), key=lambda row: (row[1][0], row[1][1], row[0]), reverse=True):
            text = text[:start] + replacement + text[end:]
        Document(text, self.comments)
        return text

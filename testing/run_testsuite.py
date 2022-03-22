from argparse import ArgumentParser
from collections import namedtuple
import chess
import subprocess

Position = namedtuple('Position', 'id fen bm')

def parse_epd(epd):
    board, params = chess.Board.from_epd(epd)
    return Position(id=params['id'], fen=board.fen(), 
        bm=params['bm'][0].uci())

def parse_line(s_line):
    if not s_line.startswith('info'):
        return None
    s_line = s_line.replace('info', '') \
        .replace('mate ', 'mate') \
        .replace('cp ', 'cp')

    info, pv = s_line.strip().split('pv')
    items = list(info.strip().split())
    d = dict(zip(items[0::2], items[1::2]))
    
    d['score'] = d['score'].replace('mate', 'mate ') \
            .replace('cp', 'cp ')

    return d, list(pv.strip().split())


def run(eng_path, positions, depth, time, debug):
    def dbg_print(s):
        if debug:
            print(s)

    eng = subprocess.Popen(eng_path, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, bufsize=1)

    eng.stdin.write('uci\n')
    eng.stdin.flush()
    dbg_print('> uci')

    line = eng.stdout.readline().strip()
    dbg_print(f'< {line}')
    while line != 'uciok':
        line = eng.stdout.readline().strip()
        if debug:
            dbg_print(f'< {line}')

    total_nodes, total_time, total_depth = 0, 0, 0
    passed = 0
    for pos in positions:
        eng.stdin.write(f'position fen {pos.fen}\n')
        eng.stdin.write(f'go movetime {time} depth {depth}\n')
        eng.stdin.flush()

        dbg_print(f'> position fen {pos.fen}')
        dbg_print(f'> go movetime {time} depth {depth}')

        d = None
        while True:
            line = eng.stdout.readline().strip()
            if line.startswith('bestmove'):
                bm = line.replace('bestmove ', '').strip()
                break
            elif line.startswith('info'):
                d, bm = parse_line(line)
                dbg_print(d)
                if bm == pos.bm:
                    eng.stdin.write('stop\n')
                    eng.stdin.flush()
                    dbg_print('> stop')
                    break
            else:
                print(f'invalid response: {line}')
                eng.kill()
                return

        if bm == pos.bm:
            verdict = '[  OK  ]'
            passed += 1
        else:
            verdict = '[ FAIL ]'
        print(f'{verdict} {pos.id}: {pos.bm} - {bm}')
        if d:
            total_time += int(d['time'])
            total_nodes += int(d['nodes'])
            total_depth += int(d['depth'])

    nps = total_nodes * 1000 // max(total_time, 1)
    print(f'passed {passed}/{len(positions)}, '
            + f'time {total_time}, nodes {total_nodes}, '
            + f'avg_nps {nps}, '
            + f'avg_depth {total_depth/len(positions)}')


def main():
    parser = ArgumentParser(description='Run testsuite against engine')
    parser.add_argument('ts_path', help='Path to epd testsuite')
    parser.add_argument('eng_path', help='Path to engine executable')
    parser.add_argument('--depth', help='Maximum search depth',
        default=64, type=int)
    parser.add_argument('--time', help='Maximum time', 
        default=10000, type=int)
    parser.add_argument('--debug', help='Print debug info',
        action='store_true')
    args = parser.parse_args()

    positions = []
    with open(args.ts_path) as ts:
        filtered = filter(lambda s: not s.isspace(), ts)
        positions = [parse_epd(epd) for epd in filtered]

    run(args.eng_path, positions, depth=args.depth, 
            time=args.time, debug=args.debug)


if __name__ == '__main__':
    main()


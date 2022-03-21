from argparse import ArgumentParser
from collections import namedtuple
import chess
import subprocess

Position = namedtuple('Position', 'id fen bm')

def parse_epd(epd):
    board, params = chess.Board.from_epd(epd)
    return Position(id=params['id'], fen=board.fen(), 
        bm=params['bm'][0])

def parse_line(s_line):
    if not s_line.startswith('info'):
        return None
    s_line = s_line.replace('info', '')

    info, pv = s_line.strip().split('pv')
    items = list(info.strip().split())
    d = dict(zip(items[0::2], items[1::2]))

    return d, list(pv.strip().split())


def run(eng_path, positions):
    eng = subprocess.Popen(eng_path, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, bufsize=1)

    eng.stdin.write('uci\n')
    eng.stdin.flush()
    print('> uci')

    line = eng.stdout.readline().strip()
    print(f'< {line}')
    while line != 'uciok':
        line = eng.stdout.readline().strip()
        print(f'< {line}')

    for pos in positions[:10]:
        eng.stdin.write(f'position fen {pos.fen}\n')
        eng.stdin.write('go movetime 10000\n')
        eng.stdin.flush()

        print(f'> position fen {pos.fen}')
        print('> go movetime 10000')

        while True:
            line = eng.stdout.readline().strip()
            print(f'< {line}')
            if line.startswith('bestmove'):
                bm = line.replace('bestmove ', '').strip()
                break
            elif line.startswith('info'):
                _, bm = parse_line(line)
                if bm == pos.bm:
                    break
            else:
                print(f'invalid response: {line}')
                eng.kill()
                return

        verdict = 'correct!' if bm == pos.bm else 'incorrect :('
        print(f'{pos.id}: {pos.bm} -- {bm} -- {verdict}')


def main():
    parser = ArgumentParser(description='Run testsuite against engine')
    parser.add_argument('ts_path', help='Path to epd testsuite')
    parser.add_argument('eng_path', help='Path to engine executable')
    args = parser.parse_args()

    with open(args.ts_path) as ts:
        positions = [parse_epd(epd) for epd in 
            filter(lambda s: not s.isspace(), ts)]

    run(args.eng_path, positions)


if __name__ == '__main__':
    main()


from argparse import ArgumentParser
from collections import namedtuple
import chess
import subprocess
from multiprocessing import Pool

Position = namedtuple('Position', 'id fen bm')
Result = namedtuple('Result', 'id expected got')
Stats = namedtuple('Stats', 'nodes time total_depth')

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

    info, s_pv = s_line.strip().split('pv')
    items = list(info.strip().split())
    d = dict(zip(items[0::2], items[1::2]))
    
    d['score'] = d['score'].replace('mate', 'mate ') \
            .replace('cp', 'cp ')

    pv = s_pv.strip().split()
    d['pv'] = pv
    return d

def run(eng_path, positions, depth, time, debug):
    eng = subprocess.Popen(eng_path, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, bufsize=1)
    output = []
    results = []

    def write_line(s):
        eng.stdin.write(s + '\n')
        eng.stdin.flush()
        if debug:
            print(f'> {s.strip()}')

    def read_line():
        line = eng.stdout.readline().strip()
        if debug:
            print(f'< {line}')
        return line
    
    write_line('uci')

    line = ''
    while line != 'uciok':
        line = read_line()

    total_nodes, total_time, total_depth = 0, 0, 0
    for pos in positions:
        write_line(f'position fen {pos.fen}')
        write_line(f'go movetime {time} depth {depth}')

        d = None
        while True:
            line = read_line()
            if line.startswith('bestmove'):
                bm = line.replace('bestmove', '').strip()
                break
            elif line.startswith('info'):
                d = parse_line(line)
                bm = d['pv'][0]
                if bm == pos.bm and int(d['depth']) > 4:
                    write_line('stop')
                    while not line.startswith('bestmove'):
                        line = read_line()
                    break
            else:
                print(f'invalid response: {line}')
                eng.kill()
                return

        results.append(Result(id=pos.id, expected=pos.bm, got=bm))
        total_time += int(d['time'])
        total_nodes += int(d['nodes'])
        total_depth += int(d['depth'])

    stats = Stats(nodes=total_nodes, time=total_time, 
            total_depth=total_depth)

    return results, stats


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
    parser.add_argument('-j', '--jobs', type=int, default=1,
        help='Number of jobs for parallel execution')
    args = parser.parse_args()

    with open(args.ts_path) as ts:
        filtered = filter(lambda s: not s.isspace(), ts)
        positions = [parse_epd(epd) for epd in filtered]

    with Pool(args.jobs) as p:
        exec_results = []
        n = len(positions) // max(args.jobs, 1)
        for i in range(0, len(positions), n):
            tpl = (args.eng_path, positions[i:i+n], 
                args.depth, args.time, args.debug)
            exec_results.append(p.apply_async(run, tpl))

        nodes, time, total_depth = 0, 0, 0
        passed = 0
        for i in exec_results:
            results, stats = i.get()

            for r in results:
                if r.expected == r.got:
                    verdict = '[  OK  ]'
                    passed += 1
                else:
                    verdict = '[ FAIL ]'
                print(f'{verdict} {r.id}: {r.expected} - {r.got}')
        
            nodes += stats.nodes
            time += stats.time
            total_depth += stats.total_depth

    nps = nodes * 1000 // max(time, 1)
    ratio = '{:.2f}'.format(passed / len(positions))
    print(f'passed {passed}/{len(positions)} [{ratio}], '
            + f'time {time}, nodes {nodes}, '
            + f'avg_nps {nps}, '
            + 'avg_depth {:.2f}'.format(total_depth/len(positions)))


if __name__ == '__main__':
    main()


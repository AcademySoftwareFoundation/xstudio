'''
Very basic, fake production data generation! Simply returns a big dictionary
representing a hierarchy of data. We use this to drive the UI data model for our
demo plugin.

The main goal here is to demonstrate execution of python from a C++ plugin.
'''
import random
import string
import pprint
from xstudio.core import Uuid

version_types = ['CG', 'TRACK', 'COMP', 'AMIM', 'LAYOUT']
artists = ["James Anderson", "Maria Rossi", "Wei Zhang", "Liam O’Connor", "Emma Johnson", "Hiroshi Tanaka", "Sofia Martinez", "Lucas Schneider", "Olivia Brown", "Noah Wilson", "Yuki Sato", "Benjamin Clark", "Charlotte Martin", "Daniel Lee", "Isabella Garcia", "Ethan Miller", "Mia Davis", "Alexander Petrov", "Ava Thompson", "Jacob White", "Chen Wang", "Samuel Taylor", "Emily Moore", "Matteo Bianchi", "Chloe Anderson", "David Kim", "Anna Kowalski", "Michael Smith", "Haruto Suzuki", "Grace Hall", "Sebastian Müller", "Lily Evans", "William Harris", "Arjun Patel", "Natalie Walker", "Jack Robinson", "Victoria Young", "Oliver King", "Elena Popescu", "Lucas Wright", "Amelia Scott", "Henry Green", "Sofia Dimitrova", "Ryan Baker", "Zoe Adams", "Maxime Dubois", "Hannah Nelson", "Gabriel Silva", "Ella Carter", "Thomas Novak", "Aiko Yamamoto", "Christopher Mitchell", "Sarah Perez", "Jonathan Turner", "Laura Fischer", "Dylan Murphy", "Isabella Costa", "Kevin Campbell", "Mei Lin", "Nathan Stewart", "Julia Weber", "Brandon Phillips", "Alina Ivanova", "Jason Parker", "Sophie Laurent", "Daniel Nguyen", "Madison Edwards", "Adrian Popov", "Rachel Collins", "Aaron Morris", "Lena Hoffmann", "Justin Rogers", "Niamh Kelly", "Eric Reed", "Katarina Horvat", "Adam Cooper", "Claire Bennett", "Felix Wagner", "Brooke Simmons", "Marcus Johansson", "Vanessa Cruz", "Owen Price", "Priya Sharma", "Cody Richardson", "Ingrid Svensson", "Tyler Gray", "Yuna Choi", "Victor Hernandez", "Paige Foster", "Kenji Nakamura", "Scott Bailey", "Anya Smirnova", "Logan Rivera", "Lucy Barnes", "Andrej Kovac", "Kayla Howard", "Marco Romano", "Leah Brooks", "Samuel Ortiz", "Taro Watanabe"]
statuses = ['final', 'pending', 'approved', 'declined', 'proposed']

def seqeunce_code(num=3):
    return ''.join(random.choices(string.ascii_lowercase, k=num)) + '_' + ''.join(random.choices(string.digits, k=num)) + '_seq'

def shot_code(seq, idx):
    return '{}_{:04d}'.format(seq.replace("_seq", ""), idx*10)

def make_version_entry(job_id, job_name, sequence_id, shot_id, shot_name, version_type, version):

    if version_type=="OTIO":
        render_range = [1001, 1001+random.randint(550,2000)]
        return {
            'uuid': str(Uuid.generate()),
            'sequence_id': sequence_id,
            'job_id': job_id,
            'shot_id': shot_id,
            'version_type': version_type,
            'version_name': '{1}_{0}_v{2:03d}'.format(shot_name, version_type, version),
            'artist': artists[random.randint(0,len(artists)-1)],
            'asset' : True,
            'status' : statuses[random.randint(0,len(statuses)-1)],
            'version' : version,
            'frame_range' : '{}-{}'.format(render_range[0], render_range[1]),
            'media_path': 'http://fake_media/{0}/{1}/{2}/{2}_{1}_v{3:03d}/{2}_{1}_v{3:03d}.otio'.format(job_name, shot_name, version_type, version)
        }
    else:
        render_range = [1001, 1001+random.randint(50,200)]
        return {
            'uuid': str(Uuid.generate()),
            'sequence_id': sequence_id,
            'job_id': job_id,
            'shot_id': shot_id,
            'version_type': version_type,
            'version_name': '{1}_{0}_v{2:03d}'.format(shot_name, version_type, version),
            'artist': artists[random.randint(0,len(artists)-1)],
            'asset' : True,
            'status' : statuses[random.randint(0,len(statuses)-1)],
            'version' : version,
            'frame_range' : '{}-{}'.format(render_range[0], render_range[1]),
            'media_path': 'http://fake_media/{0}/{1}/{2}/{2}_{1}_v{3:03d}/{2}_{1}_v{3:03d}.{4}-{5}.fake'.format(job_name, shot_name, version_type, version, render_range[0], render_range[1])
        }

def make_versions_table(jobs_table):

    versions_table = []
    for job in jobs_table['rows']:

        job_name = job['job']
        job_id = job['uuid']
        for sequence in job['rows']:

            sequence_id = sequence['uuid']

            versions_table.append(
                make_version_entry(
                    job_id,
                    job_name,
                    sequence_id,
                    shot_id=sequence_id,
                    shot_name=sequence['sequence'],
                    version_type="OTIO",
                    version=1
                ))
                
            for shot in sequence['rows']:

                shot_id = shot['uuid']
                shot_name = shot['shot']

                for version_type in version_types:

                    for version in range(1, random.randint(1,4)):

                        versions_table.append(
                            make_version_entry(
                                job_id,
                                job_name,
                                sequence_id,
                                shot_id,
                                shot_name,
                                version_type,
                                version
                            ))
    return versions_table

def make_job_seq_shot_table():

    jobs = ['ABC123', 'DRF14', 'XSTUD_45', 'KKJJ12', 'BIGMOVIE14', 'ANIMSERIES', 'FORTRESS']        
    jobs_data = {
        'level': "studio",
        'rows': []
    }
    for job in jobs:

        job_data = {
            'job': job,
            'uuid': str(Uuid.generate()),
            'level': 'job',
            'rows': []
            }

        sequences = [seqeunce_code() for i in range(1, random.randint(3,7))]
        for sequence in sequences:

            sequenence_data = {
                'sequence': sequence,
                'expanded': False,
                'level': 'sequence',
                'job': job,
                'uuid': str(Uuid.generate()),
                'rows': []
                }

            shots = [shot_code(sequence, i) for i in range(1, random.randint(3,7))]

            for shot in shots:

                shot_range = [1001, 1001+random.randint(40,200)]
                shot_range_s = "{}-{}".format(shot_range[0], shot_range[1])
                shot_data = {
                    'sequence': sequence,
                    'expanded': False,
                    'job': job,
                    'level': 'shot',
                    'shot': shot,
                    'uuid': str(Uuid.generate()),
                    'comp_range': shot_range_s,
                    'rows': []}

                sequenence_data['rows'].append(shot_data)

            job_data['rows'].append(sequenence_data)

        jobs_data['rows'].append(job_data)

    return jobs_data

def generate_tables():

    # ensure our random data is the same for each run
    random.seed(1234)
    jobs_table = make_job_seq_shot_table()
    versions_table = make_versions_table(jobs_table)
    return [jobs_table, versions_table]

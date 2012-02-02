#!/usr/bin/env python

import xml.etree.ElementTree as ET

from .tasks import TaskSystem, SporadicTask
from .resources import ResourceRequirement, ResourceRequirements
from schedcat.util.storage import storage

EPSILON = 10**-7

def maybe_int(x):
    "Try to interpret x as an integer. Convert from string, if necessary."
    if type(x) == float and abs(x) % 1 <= EPSILON:
        return int(x)
    elif type(x) == str:
        try:
            return maybe_int(float(x))
        except ValueError:
            return x
    else:
        return x

def set_attribute(tag, attr_name, obj, field_name=None):
    "Set XML attributes based on obj attributes that might not exist."
    if field_name is None:
        field_name = attr_name
    if field_name in obj.__dict__:
        tag.set(attr_name, str(obj.__dict__[field_name]))

def subtag_for_attribute(tag, obj, field_name, tag_name=None):
    if tag_name is None:
        tag_name = field_name
    if field_name in obj.__dict__:
        return ET.SubElement(tag, tag_name)
    else:
        return None

def res_requirement(r, rmodel=None):
    if rmodel is None:
        tag = ET.Element('requirement')
    else: 
        tag = ET.SubElement(rmodel, 'requirement')

    set_attribute(tag, 'res_id', r)
    set_attribute(tag, 'max_reads', r)
    set_attribute(tag, 'max_writes', r)
    set_attribute(tag, 'max_read_length', r)
    set_attribute(tag, 'max_write_length', r)

    return tag

def task(t):
    tag = ET.Element('task')
    if not t.id is None:
        set_attribute(tag, 'id', t)
    set_attribute(tag, 'period', t)
    set_attribute(tag, 'wcet', t, 'cost')
    if not t.implicit_deadline():
        set_attribute(tag, 'deadline', t)

    set_attribute(tag, 'partition', t)
    set_attribute(tag, 'response_time', t)
    set_attribute(tag, 'wss', t)

    rmodel = subtag_for_attribute(tag, t, 'resmodel', 'resources')
    if not rmodel is None:
        for res_id in t.resmodel:
            res_requirement(t.resmodel[res_id], rmodel)

    tag.task = t
    task.xml = tag
    return tag



def parse_request(req_node):
    return ResourceRequirement(
               maybe_int(req_node.get('res_id', 0)),
               int(req_node.get('max_writes', 1)),
               int(req_node.get('max_write_length', 1)),
               int(req_node.get('max_reads', 0)),
               int(req_node.get('max_read_length', 0)),
           )

def parse_resmodel(node):
    resmodel = node.find('resources')
    if resmodel != None:
        reqs = ResourceRequirements()
        for req in [parse_request(n) for n in resmodel.findall('requirement')]:
            reqs[req.res_id] = req
        return reqs
    else:
        return None

def get_attribute(node, attr_name, obj, field_name=None, convert=lambda _: _):
    if field_name is None:
        field_name = attr_name
    x = node.get(attr_name, None)
    if not x is None:
        obj.__dict__[field_name] = convert(x)
        return True
    else:
        return False

def parse_task(node):
    cost      = maybe_int(node.get('wcet'))
    period    = maybe_int(node.get('period'))

    t = SporadicTask(cost, period)

    get_attribute(node, 'deadline', t,  convert=maybe_int)
    get_attribute(node, 'id', t,  convert=maybe_int)
    get_attribute(node, 'partition', t, convert=maybe_int)
    get_attribute(node, 'wss', t, convert=maybe_int)

    resmodel = parse_resmodel(node)
    if not resmodel is None:
        t.resmodel = resmodel

    t.xml = node
    node.task = t
    return t

def taskset(ts):
    tag = ET.Element('taskset')

    prop = ET.SubElement(tag, 'properties')
    prop.set('utilization', str(ts.utilization()))
    prop.set('utilization_q', str(ts.utilization_q()))    
    prop.set('density_q', str(ts.density_q()))
    prop.set('density', str(ts.density()))
    prop.set('count', str(len(ts)))
    hp = ts.hyperperiod()
    if hp:
        prop.set('hyperperiod', str(hp))

    for t in ts:
        tag.append(task(t))
    return tag

def testpoint(tasksets, params):
    tag = ET.Element('testpoint')

    config = ET.SubElement(tag, 'config')
    for k in params:
        config.set(k, str(params[k]))
    for ts in tasksets:
        tag.append(taskset(ts))
    return tag

def parse_taskset(node):
    tasks = [parse_task(n) for n in node.findall('task')]
    return TaskSystem(tasks)

def parse_testpoint(node):
    params = {}
    config = node.find('config')
    if not config is None:
        for k in config.keys():
            params[k] = maybe_int(config.get(k))
    tss = [parse_taskset(n) for n in node.findall('taskset')]
    return (params, tss)

def write_xml(xml, fname):
    tree = ET.ElementTree(xml)
    tree.write(fname)

def write_testpoint(tasksets, params, fname):
    xml = testpoint(tasksets, params)
    write_xml(xml, fname)

def write(ts, fname):
    xml = taskset(ts)
    write_xml(xml, fname)

def load(file):
    tree = ET.ElementTree()
    tree.parse(file)
    root = tree.getroot()
    if root.tag == 'taskset':
        ts = parse_taskset(root)
        ts.xml = tree.getroot()
        return ts
    elif root.tag == 'testpoint':
        params, tss = parse_testpoint(root)
        return storage(xml=root, params=params, tasksets=tss)
    elif root.tag == 'task':
        t = parse_task(root)
        return t
    else:
        return None
